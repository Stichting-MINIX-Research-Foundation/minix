/*	$NetBSD: elink3.c,v 1.4 2008/12/14 18:46:33 christos Exp $	*/

/* stripped down from freebsd:sys/i386/netboot/3c509.c */

/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters.
  Date: Mar 22 1995

 This code is based heavily on David Greenman's if_ed.c driver and
  Andres Vega Garcia's if_ep.c driver.

 Copyright (C) 1993-1994, David Greenman, Martin Renters.
 Copyright (C) 1993-1995, Andres Vega Garcia.
 Copyright (C) 1995, Serge Babkin.
  This software may be used, modified, copied, distributed, and sold, in
  both source and binary form provided that the above copyright and these
  terms are retained. Under no circumstances are the authors responsible for
  the proper functioning of this software, nor do the authors assume any
  responsibility for damages incurred with its use.

3c509 support added by Serge Babkin (babkin@hq.icb.chel.su)

3c509.c,v 1.2 1995/05/30 07:58:52 rgrimes Exp

***************************************************************************/

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>

#include <libi386.h>

#include "etherdrv.h"
#include "3c509.h"

extern unsigned short eth_base;

extern u_char eth_myaddr[6];

void
epstop(void)
{

	/* stop card */
	outw(BASE + EP_COMMAND, RX_DISABLE);
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

	outw(BASE + EP_COMMAND, TX_DISABLE);
	outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);

	outw(BASE + EP_COMMAND, RX_RESET);
	outw(BASE + EP_COMMAND, TX_RESET);

	outw(BASE + EP_COMMAND, C_INTR_LATCH);
	outw(BASE + EP_COMMAND, SET_RD_0_MASK);
	outw(BASE + EP_COMMAND, SET_INTR_MASK);
	outw(BASE + EP_COMMAND, SET_RX_FILTER);
}

void
EtherStop(void)
{

	epstop();
	outw(BASE + EP_COMMAND, GLOBAL_RESET);
	delay(100000);
}

/**************************************************************************
ETH_RESET - Reset adapter
***************************************************************************/
void
epreset(void)
{
	int i;

	/***********************************************************
			Reset 3Com 509 card
	*************************************************************/

	epstop();

	/*
	 * initialize card
	*/
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS)
		continue;

	GO_WINDOW(0);

	/* Disable the card */
	outw(BASE + EP_W0_CONFIG_CTRL, 0);

	/* Configure IRQ to none */
	outw(BASE + EP_W0_RESOURCE_CFG, SET_IRQ(0));

	/* Enable the card */
	outw(BASE + EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);

	GO_WINDOW(2);

	/* Reload the ether_addr. */
	for (i = 0; i < 6; i++)
		outb(BASE + EP_W2_ADDR_0 + i, eth_myaddr[i]);

	outw(BASE + EP_COMMAND, RX_RESET);
	outw(BASE + EP_COMMAND, TX_RESET);

	/* Window 1 is operating window */
	GO_WINDOW(1);
	for (i = 0; i < 31; i++)
		inb(BASE + EP_W1_TX_STATUS);

	/* get rid of stray intr's */
	outw(BASE + EP_COMMAND, ACK_INTR | 0xff);

	outw(BASE + EP_COMMAND, SET_RD_0_MASK | S_5_INTS);

	outw(BASE + EP_COMMAND, SET_INTR_MASK);

	outw(BASE + EP_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
	    FIL_BRDCST);

	/* configure BNC */
	if (ether_medium == ETHERMEDIUM_BNC) {
		outw(BASE + EP_COMMAND, START_TRANSCEIVER);
		delay(1000);
	}
	/* configure UTP */
	if (ether_medium == ETHERMEDIUM_UTP) {
		GO_WINDOW(4);
		outw(BASE + EP_W4_MEDIA_TYPE, ENABLE_UTP);
		GO_WINDOW(1);
	}

	/* start tranciever and receiver */
	outw(BASE + EP_COMMAND, RX_ENABLE);
	outw(BASE + EP_COMMAND, TX_ENABLE);

	/* set early threshold for minimal packet length */
	outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | 64);

	outw(BASE + EP_COMMAND, SET_TX_START_THRESH | 16);
}

/**************************************************************************
ETH_TRANSMIT - Transmit a frame
***************************************************************************/
static const char padmap[] = {
	0, 3, 2, 1};

int
EtherSend(char *pkt, int len)
{
	int pad;
	int status;

#ifdef EDEBUG
	printf("{l=%d}", len);
#endif

	pad = padmap[len & 3];

	/*
	* The 3c509 automatically pads short packets to minimum ethernet length,
	* but we drop packets that are too large. Perhaps we should truncate
	* them instead?
	*/
	if (len + pad > ETHER_MAX_LEN) {
		return -1;
	}

	/* drop acknowledgements */
	while ((status = inb(BASE + EP_W1_TX_STATUS)) & TXS_COMPLETE ) {
		if (status & (TXS_UNDERRUN | TXS_MAX_COLLISION |
			TXS_STATUS_OVERFLOW)) {
			outw(BASE + EP_COMMAND, TX_RESET);
			outw(BASE + EP_COMMAND, TX_ENABLE);
		}

		outb(BASE + EP_W1_TX_STATUS, 0x0);
	}

	while (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
		/* no room in FIFO */
		continue;
	}

	outw(BASE + EP_W1_TX_PIO_WR_1, len);
	outw(BASE + EP_W1_TX_PIO_WR_1, 0x0);	/* Second dword meaningless */

	/* write packet */
	outsw(BASE + EP_W1_TX_PIO_WR_1, pkt, len / 2);
	if (len & 1)
		outb(BASE + EP_W1_TX_PIO_WR_1, *(pkt + len - 1));

	while (pad--)
		outb(BASE + EP_W1_TX_PIO_WR_1, 0);	/* Padding */

	/* timeout after sending */
	delay(1000);
	return len;
}

/**************************************************************************
ETH_POLL - Wait for a frame
***************************************************************************/
int
EtherReceive(char *pkt, int maxlen)
{
	/* common variables */
	int len;
	/* variables for 3C509 */
	short status, cst;
	register short rx_fifo;

	cst = inw(BASE + EP_STATUS);

#ifdef EDEBUG
	if (cst & 0x1FFF)
		printf("-%x-",cst);
#endif

	if ((cst & (S_RX_COMPLETE|S_RX_EARLY)) == 0) {
		/* acknowledge  everything */
		outw(BASE + EP_COMMAND, ACK_INTR| (cst & S_5_INTS));
		outw(BASE + EP_COMMAND, C_INTR_LATCH);

		return 0;
	}

	status = inw(BASE + EP_W1_RX_STATUS);
#ifdef EDEBUG
	printf("*%x*",status);
#endif

	if (status & ERR_RX) {
		outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
		return 0;
	}

	rx_fifo = status & RX_BYTES_MASK;
	if (rx_fifo == 0)
		return 0;

	if (rx_fifo > maxlen)
		goto zulang;

	/* read packet */
#ifdef EDEBUG
	printf("[l=%d",rx_fifo);
#endif
	insw(BASE + EP_W1_RX_PIO_RD_1, pkt, rx_fifo / 2);
	if (rx_fifo & 1)
		pkt[rx_fifo-1] = inb(BASE + EP_W1_RX_PIO_RD_1);
	len = rx_fifo;

	for (;;) {
		status = inw(BASE + EP_W1_RX_STATUS);
#ifdef EDEBUG
		printf("*%x*",status);
#endif
		rx_fifo = status & RX_BYTES_MASK;

		if (rx_fifo > 0) {
			if ((len + rx_fifo) > maxlen)
				goto zulang;

			insw(BASE + EP_W1_RX_PIO_RD_1, pkt + len, rx_fifo / 2);
			if (rx_fifo & 1)
				pkt[len + rx_fifo-1] = inb(BASE + EP_W1_RX_PIO_RD_1);
			len += rx_fifo;
#ifdef EDEBUG
			printf("+%d",rx_fifo);
#endif
		}

		if ((status & RX_INCOMPLETE) == 0) {
#ifdef EDEBUG
			printf("=%d",len);
#endif
			break;
		}

		delay(1000);
	}

	/* acknowledge reception of packet */
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS)
		continue;

	return len;

 zulang:
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS)
		continue;
	return 0;
}

/*************************************************************************
	3Com 509 - specific routines
**************************************************************************/

static int
eeprom_rdy(void)
{
	int i;

	for (i = 0; is_eeprom_busy(IS_BASE) && i < MAX_EEPROMBUSY; i++);
	if (i >= MAX_EEPROMBUSY) {
		printf("3c509: eeprom failed to come ready.\r\n");
		return 0;
	}
	return 1;
}

/*
 * get_e: gets a 16 bits word from the EEPROM. we must have set the window
 * before
 */
int
ep_get_e(int offset)
{
	if (!eeprom_rdy())
		return 0xffff;
	outw(IS_BASE + EP_W0_EEPROM_COMMAND, EEPROM_CMD_RD | offset);
	if (!eeprom_rdy())
		return 0xffff;
	return inw(IS_BASE + EP_W0_EEPROM_DATA);
}

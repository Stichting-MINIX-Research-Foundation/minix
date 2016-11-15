/* $NetBSD: ppbus_base.h,v 1.8 2008/04/15 15:02:29 cegger Exp $ */

/*
 * Copyright (c) 1997, 1998, 1999 Nicolas Souchu
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
 * $FreeBSD: src/sys/dev/ppbus/ppbconf.h,v 1.17.2.1 2000/05/24 00:20:57 n_hibma Exp $
 *
 */
#ifndef __PPBUS_BASE_H
#define __PPBUS_BASE_H

#include <sys/bus.h>

#include <dev/ppbus/ppbus_msq.h>


/* Parallel Port Chipset control bits. */
#define STROBE		0x01
#define AUTOFEED	0x02
#define nINIT		0x04
#define SELECTIN	0x08
#define IRQENABLE	0x10
#define PCD		0x20

#define nSTROBE		inv(STROBE)
#define nAUTOFEED	inv(AUTOFEED)
#define INIT		inv(nINIT)
#define nSELECTIN	inv(SELECTIN)
#define nPCD		inv(PCD)

/* Parallel Port Chipset status bits. */
#define TIMEOUT		0x01
#define nFAULT		0x08
#define SELECT		0x10
#define PERROR		0x20
#define nACK		0x40
#define nBUSY		0x80

/* Byte mode signals */
#define HOSTCLK         nSTROBE /* Also ECP mode signal */
#define HOSTBUSY        nAUTOFEED
#define ACTIVE1284      nSELECTIN /* Also ECP mode signal */
#define PTRCLK          nACK
#define PTRBUSY         BUSY
#define ACKDATAREQ      PERROR
#define XFLAG           SELECT /* Also ECP mode signal */
#define nDATAVAIL       nERROR

/* ECP mode signals */
#define HOSTACK         nAUTOFEED
#define nREVREQ         nINIT
#define PERICLK         nACK
#define PERIACK         BUSY
#define nACKREV         PERROR
#define nPERIREQ        nERROR

/* EPP mode signals */
#define nWRITE          nSTROBE
#define nDATASTB        nAUTOFEED
#define nADDRSTB        nSELECTIN
#define nWAIT           BUSY
/*
#define nINIT           nRESET
#define nACK            nINTR
*/

/* ECR register bit definitions */
#define ECR_FIFO_EMPTY        0x1     /* ecr register - bit 0 */
#define ECR_FIFO_FULL         0x2     /* ecr register - bit 1 */
#define ECR_SERVICE_INTR      0x4     /* ecr register - bit 2 */
#define ECR_ENABLE_DMA        0x8     /* ecr register - bit 3 */
#define ECR_nFAULT_INTR       0x10    /* ecr register - bit 4 */
/* bits 5 through 7 */
#define ECR_STD           0x00    /* Standard mode */
#define ECR_PS2           0x20    /* Bidirectional mode */
#define ECR_FIFO          0x40    /* Fast Centronics mode */
#define ECR_ECP           0x60    /* ECP mode */
#define ECR_EPP           0x80    /* EPP mode */
#define ECR_TST           0xc0    /* Test mode*/

/* Used for IEEE 1284 'PNP' detection */
#define PPBUS_PNP_PRINTER	0
#define PPBUS_PNP_MODEM		1
#define PPBUS_PNP_NET		2
#define PPBUS_PNP_HDC		3
#define PPBUS_PNP_PCMCIA	4
#define PPBUS_PNP_MEDIA		5
#define PPBUS_PNP_FDC		6
#define PPBUS_PNP_PORTS		7
#define PPBUS_PNP_SCANNER	8
#define PPBUS_PNP_DIGICAM	9
#define PPBUS_PNP_UNKNOWN	10


/* Structure to store status information. */
struct ppbus_status {
	unsigned char status;

	unsigned int timeout:1;
	unsigned int error:1;
	unsigned int select:1;
	unsigned int paper_end:1;
	unsigned int ack:1;
	unsigned int busy:1;
};

/* How tsleep() is called in ppbus_request_bus(). */
#define PPBUS_DONTWAIT  0
#define PPBUS_NOINTR    0
#define PPBUS_WAIT      0x1
#define PPBUS_INTR      0x2
#define PPBUS_POLL      0x4
#define PPBUS_FOREVER   -1


/* PPBUS interface functions (includes parport interface) */
int ppbus_scan_bus(device_t);
void ppbus_pnp_detect(device_t);
int ppbus_request_bus(device_t, device_t, int, unsigned int);
int ppbus_release_bus(device_t, device_t, int, unsigned int);
int ppbus_get_status(device_t, struct ppbus_status *);
int ppbus_poll_bus(device_t, int, char, char, int);

/* Parport interface function prototypes */
int ppbus_read_ivar(device_t, int, unsigned int *);
int ppbus_write_ivar(device_t, int, unsigned int *);
int ppbus_reset_epp_timeout(device_t);
int ppbus_ecp_sync(device_t);
int ppbus_set_mode(device_t, int, int);
int ppbus_get_mode(device_t);
int ppbus_write(device_t, char *, int, int, size_t *);
int ppbus_read(device_t, char *, int, int, size_t *);
int ppbus_exec_microseq(device_t, struct ppbus_microseq * *);
int ppbus_io(device_t, int, u_char *, int, u_char);
int ppbus_dma_malloc(device_t, void **, bus_addr_t *, bus_size_t);
int ppbus_dma_free(device_t, void **, bus_addr_t *, bus_size_t);
int ppbus_add_handler(device_t, void (*)(void *), void *);
int ppbus_remove_handler(device_t, void (*)(void *));

#endif /* __PPBUS_BASE_H */

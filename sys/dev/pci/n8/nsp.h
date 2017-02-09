/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NSP_H

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcidevs.h>
#include <opencrypto/cryptodev.h>
#include "n8_driver_main.h"
#include "n8_pub_symmetric.h"
#include "n8_pub_packet.h"
#include "config.h"

#ifndef PCI_VENDOR_NETOCTAVE
#define PCI_VENDOR_NETOCTAVE 0x170b
#endif
#ifndef PCI_PRODUCT_NETOCTAVE_NSP2000
#define PCI_PRODUCT_NETOCTAVE_NSP2000 0x100
#endif

#define NSP_BAR0		(PCI_MAPREG_START + 0)	/* PUC register map */
#define NSP_BAR1		(PCI_MAPREG_START + 4)	/* DMA register map */
#define	NSP_TRDY_TIMEOUT	0x40	/* TRDY timeout */
#define	NSP_RETRY_TIMEOUT	0x41	/* DMA retry timeout */

/* DEBUG MESSAGE BIT SELECTORS */
#define N8_DBG_GENERAL            1
#define N8_DBG_IRQ                2
#define N8_DBG_BIGALLOC           4
#define N8_DBG_DISPLAY_MEMORY     8
#define N8_DBG_DISPLAY_CONTEXT    16
#define N8_DBG_DISPLAY_REGISTERS  32
#define N8_DBG_DISPLAY_QMGR       64

#define DEBUG_GENERAL     (N8_Debug_g & N8_DBG_GENERAL)
#define DEBUG_IRQ         (N8_Debug_g & N8_DBG_IRQ)
#define DEBUG_BIGALLOC    (N8_Debug_g & N8_DBG_BIGALLOC)
#define APRINT            if (DEBUG_GENERAL) printf

/* The following two values are inSilicon PCI test registers that need  */
/* to be set to the PCI standards rather than their defaults.           */
#define INSILICON_PCI_TRDY_TIMEOUT      0x40 
#define INSILICON_PCI_RETRY_TIMEOUT     0x41

#define NSP_MAX_SESSION		1024
#define NSP_MAX_KEY_LEN		(256*8)
#define NSP_MAX_KEY_BYTES	((NSP_MAX_KEY_LEN+7)/8)

#define NSP_STATE_INIT		0

typedef struct nsp_session {
	uint32_t magic;
	uint32_t sid;
	struct nsp_softc *sc;
	N8_ContextHandle_t contextHandle;	/* Context memory handle */
	int contextAllocated;			/* Boolean flag for cleanup of context */

	N8_EncryptObject_t crypt;		/* Encrypt/Decrypt control data */
	N8_EncryptCipher_t cipherInfo;
	uint8_t iv[16];

	uint8_t mackey[64];
	uint8_t mackeylen;

	N8_HashObject_t hash;

	void *crp_opaque;			/* user data to return with result */
	struct cryptop *crp;			/* complete crypto op */
	struct cryptodesc *crd;			/* active crypto op */
	int crd_id;				/* active crd index */

	int active;				/* Boolean: operation active on session */
	int state;

	union {
	    struct mbuf *mb;
	    struct uio  *io;
	    N8_Buffer_t *ptr;
	} src;
	union {
	    struct mbuf *mb;
	    struct uio  *io;
	    N8_Buffer_t *ptr;
	} dst;
        N8_Buffer_t *mac;			/* IOV mode means direct copy for hashes.... !? */

	struct nsp_session *next;		/* next free */
} nsp_session_t;

struct nsp_softc {
	device_t		sc_dev;
	pci_chipset_tag_t	pa_pc;
	pcitag_t		pa_tag;
	void			*int_handle;
	kmutex_t		sc_intrlock;

	bus_space_handle_t	mem_handle;
	bus_space_tag_t		mem_tag;
	bus_size_t		mem_size;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dmamap;
	int			mem_mapped;
	uint32_t		flags;
	int			unit;
	int			sc_suspended;
	int			sc_needwakeup;	/* notify crypto layer */
	u_int32_t		sc_statmask;	/* interrupt status mask */
	int32_t			cid;		/* crypto tag */
	NspInstance_t		*nip;
	int		        usage_count;
	nsp_session_t		session[NSP_MAX_SESSION];
	nsp_session_t		*freesession;	/* first free session */
};

#endif /* _NSP_H */

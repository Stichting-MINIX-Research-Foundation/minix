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

/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 * nsp.c - NetOctave NSP2000 NetBSD OpenCrypto Driver
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>
#include <sys/md5.h>
#include <sys/sha1.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include "n8_driver_main.h"
#include "n8_driver_api.h"
#include "config.h"
#include "nsp.h"
#include "irq.h"
#include "n8_pub_rng.h"
#include "n8_pub_hash.h"
#include "n8_pub_symmetric.h"
#include "n8_pub_context.h"
#include "n8_pub_pk.h"

#include "nspvar.h"

static int nsp_probe(device_t parent, cfdata_t match, void *aux);
static void nsp_attach(device_t parent, device_t self, void *aux);
static int nsp_detach(device_t dev, int flags);


#ifdef _MODULE
CFATTACH_DECL_NEW(nsp2000, sizeof(struct nsp_softc), nsp_probe, nsp_attach, nsp_detach, NULL);

int nsp2000_lkmentry(struct lkm_table *lkmtp, int cmd, int ver);
CFDRIVER_DEC(nsp2000, DV_DULL, NULL);
extern struct cfdriver nsp2000_cd;
extern struct cfattach nsp2000_ca;
static int pciloc[] = { -1, -1 }; /* device, function */
static struct cfparent pciparent = {
	"pci", "pci", DVUNIT_ANY
};
static struct cfdata nsp2000_cfdata[] = {
	{"nsp2000", "nsp2000", 0, FSTATE_STAR, pciloc, 0, &pciparent},
	{ 0, 0, 0, 0, 0, 0, 0 }
};

static struct cfdriver *nsp2000_cfdrivers[] = {
	&nsp2000_cd,
	NULL
};
static struct cfattach *nsp2000_cfattachs[] = {
	&nsp2000_ca,
	NULL
};
static const struct cfattachlkminit nsp2000_cfattachinit[] = {
	{ "nsp2000", nsp2000_cfattachs },
	{ NULL, NULL }
};

MOD_DRV("nsp2000", nsp2000_cfdrivers, nsp2000_cfattachinit, nsp2000_cfdata);

int
nsp2000_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
{
	/* LKM_DISPATCH(lkmtp, cmd, NULL, att, det, stat) */
	LKM_DISPATCH(lkmtp, cmd, NULL, lkm_nofunc, lkm_nofunc, lkm_nofunc);

}
#else /* _MODULE */
CFATTACH_DECL_NEW(nsp, sizeof(struct nsp_softc), nsp_probe, nsp_attach, nsp_detach, NULL);
#endif

static int nsp_intr(void *arg);
static int n8_process(void *arg, struct cryptop *crp, int hint);
static int n8_kprocess(void *arg, struct cryptkop *krp, int hint);
static int n8_freesession(void *arg, u_int64_t tid);
static int n8_newsession(void *arg, u_int32_t *sidp, struct cryptoini *cri);
static void n8_callback(void *data, N8_Status_t status);
static int n8_do_crypt(struct nsp_softc *sc,
	struct cryptop *crp,
	struct cryptodesc *crd,
	nsp_session_t *session);
static int n8_do_hash(struct nsp_softc *sc,
	struct cryptop *crp,
	struct cryptodesc *crd,
	nsp_session_t *session);
static int n8_start_crd(struct nsp_softc *sc,
	struct cryptop *crp,
	struct cryptodesc *crd,
	int crd_id,
	nsp_session_t *session);

static nsp_session_t *n8_session_alloc(struct nsp_softc *sc);
static void n8_session_free(struct nsp_softc *sc,
		nsp_session_t *session);

/* Supported crypto algorithms */
static struct {
	int id;
	const char *name;
} nsp_algo[] = {
	{ CRYPTO_DES_CBC,	"CRYPTO_DES_CBC" },
	{ CRYPTO_3DES_CBC,	"CRYPTO_3DES_CBC" },
	{ CRYPTO_MD5,		"CRYPTO_MD5" },
	{ CRYPTO_SHA1,		"CRYPTO_SHA1" },
	{ CRYPTO_MD5_HMAC,	"CRYPTO_MD5_HMAC" },
	{ CRYPTO_SHA1_HMAC,	"CRYPTO_SHA1_HMAC" },
	{ 0, NULL },
};

/* Supported key operations */
static struct {
	int id;
	const char *name;
} nsp_key[] = {
	{ CRK_MOD_EXP,		"CRK_MOD_EXP" },
	{ CRK_DSA_SIGN,		"CRK_DSA_SIGN" },
	{ CRK_DSA_VERIFY,	"CRK_DSA_VERIFY" },
	{ CRK_DH_COMPUTE_KEY,	"CRK_DH_COMPUTE_KEY" },
	{ CRK_MOD_ADD,		"CRK_MOD_ADD" },
	{ CRK_MOD_ADDINV,	"CRK_MOD_ADDINV" },
	{ CRK_MOD_SUB,		"CRK_MOD_SUB" },
	{ CRK_MOD_MULT,		"CRK_MOD_MULT" },
	{ CRK_MOD_MULTINV,	"CRK_MOD_MULTINV" },
	{ CRK_MOD,		"CRK_MOD" },
	{ 0, NULL },
};

/* parameter handling definitions for each supported crypto key operation */
static struct {
	const char *name;
	int iparmcount;		/* number of input parameters */
	int oparmcount;		/* number of output parameters */
	uint32_t bignums;	/* bit mask of bignum parameters needing conversion */
} crk_def[CRK_ALGORITHM_MAX+1] = {
	{ "CRK_MOD_EXP",	3,	1, NSP_MOD_EXP_BIGNUMS },
	{ NULL,			6,	1, NSP_MOD_EXP_CRT_BIGNUMS },	/* N/A */
	//{ "CRK_MOD_EXP_CRT",	6,	1, NSP_MOD_EXP_CRT_BIGNUMS },
	{ "CRK_DSA_SIGN",	5,	2, NSP_DSA_SIGN_BIGNUMS },
	{ "CRK_DSA_VERIFY",	7,	0, NSP_DSA_VERIFY_BIGNUMS },
	{ "CRK_DH_COMPUTE_KEY",	3,	1, NSP_DH_COMPUTE_KEY_BIGNUMS },
	{ "CRK_MOD_ADD",	3,	1, NSP_MOD_ADD_BIGNUMS },
	{ "CRK_MOD_ADDINV",	2,	1, NSP_MOD_ADDINV_BIGNUMS },
	{ "CRK_MOD_SUB",	3,	1, NSP_MOD_SUB_BIGNUMS },
	{ "CRK_MOD_MULT",	3,	1, NSP_MOD_MULT_BIGNUMS },
	{ "CRK_MOD_MULTINV",	2,	1, NSP_MOD_MULTINV_BIGNUMS },
	{ "CRK_MOD",		2,	1, NSP_MODULUS_BIGNUMS },
};


unsigned char	N8_Debug_g = 0;
int		NSPcount_g;
NspInstance_t	NSPDeviceTable_g[NSP_MAX_INSTANCES];

static const struct nsp_product {
	pci_vendor_id_t		nsp_vendor;
	pci_product_id_t	nsp_product;
	int			nsp_flags;
	const char		*nsp_name;
} nsp_products[] = {
	{ PCI_VENDOR_NETOCTAVE, PCI_PRODUCT_NETOCTAVE_NSP2000,
	  0,
	  "NetOctave NSP2000"
	},

	{ 0,			0,
	  0,
	  NULL
	}
};

static const struct nsp_product *
nsp_lookup(const struct pci_attach_args *pa)
{
	const struct nsp_product *nspp;

	for (nspp = nsp_products; nspp->nsp_name != NULL; nspp++) {
		if (PCI_VENDOR(pa->pa_id) == nspp->nsp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == nspp->nsp_product)
			return nspp;
	}
	return NULL;
}

static int
nsp_probe(device_t parent, cfdata_t match, void *aux)
{
	static int		once=0;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

       if (!once++) {
	  /* Here we set the NSPcount_g to be ONE so our NSP2k queue allocation */
	  /* multiplier will not be zero. The linux and FreeBSD drivers differ  */
	  /* in WHEN they increment the NSPcount_g variable. The count is taken */
	  /* at module load time under linux, but during the attach() in FBSD.  */
	  /* FBSD doesn't offer an interface to walk the existing PCI device    */
	  /* tree to count each N8 device. Code can be added to the kernel in   */
	  /* /usr/src/sys/pci/pci.c to perform this function and allow the NSP  */
	  /* queue sizes to be dynamically allocated based on the number of     */
	  /* installed chips or boards. See Docs for details */
#if 0
	  NSPcount_g = 1;
	  if (nsp_driverInit(N8_EA_POOL_SIZE, N8_PK_POOL_SIZE))
	  {
	     NSPcount_g = 0;
	     return ENOMEM;
	  }
#endif
		NSPcount_g = 0; /* reset to zero since we increment in attach */
	}
	if (nsp_lookup(pa) != NULL)
		return 1;

	return 0;
}


/* initialize the session array into a free list of
 * available sessions.
 */
static void 
n8_sessionInit(struct nsp_softc *sc)
{
	int ind;

	for (ind=0; ind<NSP_MAX_SESSION-1; ind++) {
	    sc->session[ind].next = &sc->session[ind+1];
	}
	sc->session[NSP_MAX_SESSION-1].next = NULL;

	sc->freesession = &sc->session[0];
}

static void
nsp_attach(device_t parent, device_t self, void *aux)
{
	struct nsp_softc *sc;
	struct pci_attach_args *pa = aux;
	const struct nsp_product *nspp;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	NspInstance_t	*nip  = &NSPDeviceTable_g[0];	/* can only attach once */
	u_int32_t cmd;
	int res;
	int ind;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;

	mutex_init(&sc->sc_intrlock, MUTEX_DEFAULT, IPL_NET);

	sc->pa_pc = pa->pa_pc;
	sc->pa_tag = pa->pa_tag;
	sc->dma_tag = pa->pa_dmat;

	DBG(("sc->dma_tag = 0x%x\n", (uint32_t)sc->dma_tag));
	nspp = nsp_lookup(pa);
	if (nspp == NULL) {
		DBG(("\n"));
		panic("nsp_attach: impossible");
	}

	sc->mem_mapped = 0;
	nip->dev = self;

	aprint_naive(": Crypto processor\n");
	aprint_normal(": %s, rev. %d\n", nspp->nsp_name,
	    PCI_REVISION(pa->pa_class));

	printf("NetOctave Encryption Processor - %s\n", device_xname(self));

	n8_sessionInit(sc);

	NSPcount_g = 1;
	if (n8_driverInit(N8_EA_POOL_SIZE, N8_PK_POOL_SIZE)) {
	    DBG(("%s: Failed driver init\n", device_xname(self)));
	    NSPcount_g = 0;
	    return;
	}
	DBG(("n8_driverInit complete\n"));
	/* reset to zero for rest of attach */
	NSPcount_g = 0;

	/*
	 * Map control/status registers.
	 */

	/* reset the TRDY timeout and RETRY timeout to reasonable values */
	/* note: INSILICON_PCI_RETRY_TIMEOUT is the second byte */
	cmd = pci_conf_read(sc->pa_pc, sc->pa_tag, INSILICON_PCI_TRDY_TIMEOUT);
	cmd = (cmd & 0x0000FFFF);	/* Zero TRDY_TIMEOUT and RETRY_TIMEOUT */
	pci_conf_write(sc->pa_pc, sc->pa_tag, INSILICON_PCI_TRDY_TIMEOUT, cmd);

	cmd = pci_conf_read(sc->pa_pc, sc->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(sc->pa_pc, sc->pa_tag, PCI_COMMAND_STATUS_REG, cmd);

	cmd = pci_conf_read(sc->pa_pc, sc->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
	    DBG(("nsp%d: failed to enable memory mapping!\n", sc->unit));
	    goto fail;
	}

	if (pci_mapreg_map(pa, NSP_BAR0, PCI_MAPREG_MEM_TYPE_64BIT, 0,
		&sc->mem_tag, &sc->mem_handle, NULL, &sc->mem_size)) {
		aprint_error_dev(self, "can't map mem space %d\n", 0);
		return;
	}

	sc->mem_mapped = 1;

	DBG(("nsp pci register space mapped\n"));

	/*
	 * Allocate interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error("nsp%d: couldn't map interrupt\n",
		    sc->unit);
		goto fail;
	}
	intrstr = pci_intr_string(sc->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->int_handle = pci_intr_establish(sc->pa_pc, ih, IPL_NET,
					nsp_intr, sc);
	if (sc->int_handle == NULL) {
		aprint_error("nsp%d: couldn't establish interrupt", sc->unit);
		if (intrstr != NULL)
			aprint_normal(" at %s", intrstr);
		aprint_normal("\n");
		goto fail;
	}
	aprint_normal("nsp%d: interrupting at %s\n", sc->unit, intrstr);

	/* setup card */

	memset(nip, 0, sizeof (*nip));

	sc->nip = nip;
	nip->dev = sc;
	nip->chip = NSPcount_g;

	nip->PCIinfo.vendor_id   = PCI_VENDOR(pa->pa_id);
	nip->PCIinfo.device_id   = PCI_PRODUCT(pa->pa_id);
	nip->PCIinfo.revision_id = 
	    PCI_REVISION(pci_conf_read(sc->pa_pc, sc->pa_tag, PCI_CLASS_REG));

#ifdef N8_ZERO_CACHE_LINE
	/* ENSURE Cache line size is zero in PCI CONFIG SPACE */
	/* This forces the chip to use PCI MEM_READ and not */
	/* MEM_READ_MULTIPLE - this may impact performance */

	pci_write_config(dev, PCIR_CACHELNSZ, 0x00, 1);
#endif

	nip->PCIinfo.base_address[0] = sc->mem_handle;
	nip->PCIinfo.base_range  [0] = sc->mem_handle + DEF_ASIC_PCI_MEMORY_SIZE_2;

	nip->NSPregs_base = nip->PCIinfo.base_address[0];
	nip->NSPregs_p    = (unsigned long*)nip->NSPregs_base;

	DBG(("nsp%d: calling n8_chipInit\n", sc->unit));
	if (!n8_chipInit(nip, nip->chip, N8_DEF_CMD_QUE_EXP, DEBUG_GENERAL)) {
	    DBG(("NSP2000: Failed to allocate resources for device #%d.\n",
		   nip->chip));
		goto fail_int;
	}

	DBG(("NSP2000: Configured NSP2000 ASIC #%d @%08lx (%ld MB CtxMem).\n",
	       nip->chip, nip->NSPregs_base,
	       (nip->contextMemSize / 1024) / 1024));


	sc->cid = crypto_get_driverid(0);
	if (sc->cid < 0) {
		DBG(("nsp%d: couldn't get crypto driver id\n",
			sc->unit));
		n8_driverRemove();
		goto fail_int;
	}

	/* Increment our device count. */
	NSPcount_g++;

	/* Configure/enable interrupts. */
	n8_enableInterrupts(N8_AMBA_TIMER_PRESET);

	/* Register crypto support with opencrypto */
	DBG(("nsp_attach: registering with opencrypto\n"));
	for (ind=0; nsp_algo[ind].id != 0; ind++) {
		res = crypto_register(sc->cid, nsp_algo[ind].id, 0, 0,
			    n8_newsession, n8_freesession, n8_process, sc);
		if (res < 0) {
		    DBG(("nsp_attach: failed to register %s, err=%d\n",
				    nsp_algo[ind].name, res));
		} else {
			DBG(("nsp_attach: registered %s\n",
					nsp_algo[ind].name));
		}
	}

	/* XXX use crk_def instead, or combine nsp_key and crk_def */
	for (ind=0; nsp_key[ind].name != NULL; ind++) {
		res = crypto_kregister(sc->cid, nsp_key[ind].id, 0,
			    n8_kprocess, sc);
		if (res < 0) {
		    DBG(("nsp_attach: failed to kregister %s, err=%d\n",
				    nsp_key[ind].name, res));
		} else {
			DBG(("nsp_attach: kregistered %s\n",
					nsp_key[ind].name));
		}
	}

	DBG(("nsp_attach: ready\n"));
	return;
fail_int:
	pci_intr_disestablish(sc->pa_pc, sc->int_handle);
	sc->int_handle = NULL;
fail:
	bus_space_unmap(sc->mem_tag, sc->mem_handle, sc->mem_size);
	sc->mem_mapped = 0;
	return;
}


int
nsp_detach(device_t dev, int flags)
{
	struct nsp_softc *sc;
	int res;
	int ind;

	sc = device_private(dev);
	mutex_enter(&sc->sc_intrlock);
	DBG(("nsp.%d detach\n", sc->unit));

	DBG(("nsp_detach: unregistering with opencrypto\n"));
	for (ind=0; nsp_algo[ind].id != 0; ind++) {
		res = crypto_unregister(sc->cid, nsp_algo[ind].id);
		if (res < 0) {
		    DBG(("nsp_attach: failed to unregister %s, err=%d\n",
				    nsp_algo[ind].name, res));
		} else {
			DBG(("nsp_attach: unregistered %s\n",
					nsp_algo[ind].name));
		}
	}

	n8_disableInterrupts();
	if (sc->int_handle != NULL) {
		pci_intr_disestablish(sc->pa_pc, sc->int_handle);
		DBG(("nsp.%d intr disestablished\n", sc->unit));
	} else {
		DBG(("nsp.%d no intr registered\n", sc->unit));
	}

	DBG(("nsp.%d: calling n8_driverRemove()\n", sc->unit));
	n8_driverRemove();

	if (sc->mem_mapped) {
	    DBG(("nsp.%d unmapping PCI memory\n", sc->unit));
	    bus_space_unmap(sc->mem_tag, sc->mem_handle, sc->mem_size);
	} else {
		DBG(("nsp.%d no memory mapped\n", sc->unit));
	}
        mutex_exit(&sc->sc_intrlock);
	return 0;
}

static int
nsp_intr(void *arg)
{
	struct nsp_softc *sc = arg;
	NspInstance_t	 *nip	= sc->nip;

	mutex_enter(&sc->sc_intrlock);
	DBG(("nsp_intr: handler fired\n"));
	n8_MainInterruptHandler(nip);
	mutex_exit(&sc->sc_intrlock);

	return 1;
} 

N8_Status_t N8_GetRandomBytes(int num_bytes, char *buf_p, N8_Event_t *event_p)
{
   RN_Request_t        rn_request;   /* RNG request structure */
   N8_Status_t          ret;
   DBG(("N8_GetRandomBytes\n"));

    /* check the number of bytes requested
    it can't be less or equal to 0 or more than N8_RNG_MAX_REQUEST. */
    if ((num_bytes <= 0)  || (num_bytes > N8_RNG_MAX_REQUEST)) {
	DBG(("Number of bytes requested is out of range : %d\n", num_bytes));
	DBG(("N8_GetRandomBytes - return Error\n"));
	return N8_INVALID_INPUT_SIZE;
    }

    rn_request.userRequest  = N8_FALSE;
    rn_request.userBuffer_p = buf_p;
    rn_request.numBytesRequested = num_bytes;

    /* we have a valid request.  queue it up. */
    ret = Queue_RN_request(&rn_request);
    if (ret != N8_STATUS_OK) {
	DBG(("Queue_RN_request failed\n"));
	return ret;
    }

    if (event_p != NULL) {
	event_p->unit = N8_RNG;
	event_p->state = NULL;
	event_p->status = N8_QUEUE_REQUEST_FINISHED;
    }

   return ret; 

} /* N8_GetRandomBytes */


N8_Status_t
n8_gettime( n8_timeval_t *n8_timeResults_p )
{

   struct timespec  ts;
   N8_Status_t returnResults = N8_STATUS_OK;

   getnanotime(&ts);

   /* Timespec has a seconds portion and a nano seconds portion.        */
   /* Thus we need to divide to convert nanoseconds to microseconds.    */
   n8_timeResults_p->tv_sec = ts.tv_sec;
   n8_timeResults_p->tv_usec = ts.tv_nsec / 1000;

   return returnResults;

} /* n8_gettime */

/* map an N8 Status error to a semi-appropriate errno */
static int
n8_map_errno(N8_Status_t error)
{
	switch (error) {
	case N8_STATUS_OK:
		return 0;
	case N8_INVALID_INPUT_SIZE:
	case N8_INVALID_OUTPUT_SIZE:
	case N8_INVALID_KEY_SIZE:
		return ERANGE;
	case N8_INVALID_ENUM:
		return EDOM;
	case N8_INVALID_PARAMETER:
	case N8_INVALID_OBJECT:
		/* coding bug */
		break;
	case N8_INVALID_PROTOCOL:
		return EPROTOTYPE;
	case N8_INVALID_KEY:
	case N8_INVALID_CIPHER:
	case N8_INVALID_HASH:
	case N8_INVALID_VALUE:
		return EINVAL;
	case N8_WEAK_KEY:
		break;
	case N8_UNIMPLEMENTED_FUNCTION:
		return ENODEV;
	case N8_INCONSISTENT:
		return EINVAL;
	case N8_NO_MORE_RESOURCE:
		return EBUSY;

	case N8_MALLOC_FAILED:
		return ENOMEM;

	case N8_INVALID_VERSION:
	case N8_NOT_INITIALIZED:
	case N8_UNALLOCATED_CONTEXT:
	case N8_HARDWARE_ERROR:
	case N8_UNEXPECTED_ERROR:
		return EIO;
	default:
		break;
	}

	return EINVAL;
}


/* allocate a session for a crypto op */
static nsp_session_t *
n8_session_alloc(struct nsp_softc *sc)
{
	nsp_session_t *session;

	mutex_enter(&sc->sc_intrlock);
	session = sc->freesession;
	if (sc->freesession == NULL) {
		printf("n8_newsession(): out of sessions (max = %d)\n",
			NSP_MAX_SESSION);
		mutex_exit(&sc->sc_intrlock);
		return NULL;
	}
	sc->freesession = session->next;
	session->next = NULL;
	session->magic = 0xDEADFEED;
	mutex_exit(&sc->sc_intrlock);

	session->contextAllocated = 0;
	session->active = 0;

	return session;
}

/* free a crypto op session */
static void
n8_session_free(struct nsp_softc *sc, nsp_session_t *session)
{
	if (session->contextAllocated) {
		N8_FreeContext(session->contextHandle, NULL);
		session->contextAllocated = 0;
	}

	mutex_enter(&sc->sc_intrlock);
    	if (session == NULL) {
		DBG(("n8_session_free: attempt to free NULL\n"));
		mutex_exit(&sc->sc_intrlock);
		return;
	}

	session->next = sc->freesession;
	sc->freesession = session;
	session->magic = 0xFEED;
	mutex_exit(&sc->sc_intrlock);
}


/**********************************************************************
* FUNCTION:	n8_newsession(*arg, *sidp, *cri)
* DESCRIPTION:	The opencrypto new session handler.  This function
* 		creates and initializes a new crypto session.
* 		The session can then be used to process crypto
* 		cipher and hash operations vi n8_process().
* 		Initial information is stored in the session
* 		at this time (e.g. keys, IVs).
* INPUTS:	arg	- N8 device softc
* 		cri	- crypto cipher/hash initializtion info
* OUTPUTS:	sidp	- the id for the session. An arbitrary 
* 			  integer managed by us (in this case the 
* 			  index into the session array). This will
* 			  be supplied to n8_process() to identify the
* 			  session.
* RETURNS:	0 	- ok
* 		else an errno value
* NOTES:	This is a synchronous function - no async ops are
* 		started on the N8.  These are all handled in the
* 		later n8_process() call.
**********************************************************************/
static int
n8_newsession(void *arg, u_int32_t *sidp, struct cryptoini *cri)
{
	struct cryptoini *cinit;
	struct cryptoini *cinit_crypto=NULL;
	struct cryptoini *cinit_hash=NULL;
	struct nsp_softc *sc = arg;
	nsp_session_t *session;
	int res;

	if (sc == NULL) {
	    DBG(("n8_newsession(): sc == NULL\n"));
	    return EINVAL;
	}

	for (cinit=cri; cinit != NULL; cinit = cinit->cri_next) {
		switch (cinit->cri_alg) {
		case CRYPTO_MD5:
		case CRYPTO_SHA1:
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
			if (cinit_hash != NULL) {
			    DBG(("n8_newsession(): ERROR - multiple hash requests\n"));
			    return EINVAL;
			}
			cinit_hash = cinit;
			break;
		case CRYPTO_AES_CBC:
			DBG(("n8_newsession(): invalid crypto op %d\n", cinit->cri_alg));
			return (EINVAL);
			break;
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
			if (cinit_crypto != NULL) {
			    DBG(("n8_newsession(): ERROR - multiple crypto requests\n"));
			    return EINVAL;
			}
			cinit_crypto = cinit;
			break;
		default:
			return (EINVAL);
		}
	}
	if ((cinit_crypto == NULL) && (cinit_hash == NULL)) {
		DBG(("n8_newsession(): no supported crypto ops\n"));
		return EINVAL;
	}

	session = n8_session_alloc(sc);
	if (session == NULL) {
		DBG(("n8_newsession(): out of sessions (max = %d)\n",
			NSP_MAX_SESSION));
		return ENOMEM;
	}

	if (cinit_crypto != NULL) {

		/* get a context for this session */
		/* XXX not needed but faster with or without? */
		res = N8_AllocateContext(&session->contextHandle, N8_ANY_UNIT);
		if (res != N8_STATUS_OK) {
		    DBG(("%s.%d: failed to allocate context, err=%d\n",
			    __FILE__, __LINE__, res));
		    n8_session_free(sc, session);
		    return ENOMEM;
		}

		session->contextAllocated = 1;

		/* use the same unit that the context came from */
		session->cipherInfo.unitID = session->contextHandle.unitID;
		DBG(("n8_newsession: 3DES assigned to n8 unit %d\n",
			session->contextHandle.unitID));
		session->cipherInfo.keySize = DES_KEY_SIZE_BYTES;

		/* get IV and key for DES/3DES */
		if (cinit_crypto->cri_klen > 0) {

			memcpy(session->iv, &cinit_crypto->cri_iv[0], 8);

			if (cinit_crypto->cri_alg == CRYPTO_DES_CBC) {
				DBG(("n8_newsession: setup CRYPTO_DES_CBC\n"));
				if (cinit_crypto->cri_klen != 64) {
					DBG(("n8_newsession(): des key len %d != 64\n",
						cinit_crypto->cri_klen));
					n8_session_free(sc, session);
					return EINVAL;
				}
				/* repeat the key 3 times for single DES */
				memcpy(session->cipherInfo.key.keyDES.key1,
					&cinit_crypto->cri_key[0], 8);
				memcpy(session->cipherInfo.key.keyDES.key2,
					&cinit_crypto->cri_key[0], 8);
				memcpy(session->cipherInfo.key.keyDES.key3,
					&cinit_crypto->cri_key[0], 8);
			} else {
				DBG(("n8_newsession: setup CRYPTO_3DES_CBC\n"));
				if (cinit_crypto->cri_klen != 192) {
					DBG(("n8_newsession(): 3des key len %d != 192\n",
						cinit_crypto->cri_klen));
					n8_session_free(sc, session);
					return EINVAL;
				}
				memcpy(session->cipherInfo.key.keyDES.key1,
					&cinit_crypto->cri_key[0], 8);
				memcpy(session->cipherInfo.key.keyDES.key2,
					&cinit_crypto->cri_key[8], 8);
				memcpy(session->cipherInfo.key.keyDES.key3,
					&cinit_crypto->cri_key[16], 8);
			}

		}
	}

	if (cinit_hash != NULL) {
		/* setup for hashing */
		if (cinit_hash->cri_key != NULL) {
			/* take a copy of the key for hmac */
			session->mackeylen = (cinit_hash->cri_klen+7)/8;
			memcpy(session->mackey,
				&cinit_hash->cri_key[0],
				session->mackeylen);
			DBG(("n8_newsession: stored hmac key %d bytes\n", session->mackeylen));
		} else {
			session->mackeylen = 0;
		}
	}

	/* return the index for the session */
	*sidp = session - sc->session;
	session->sc = sc;
	session->sid = *sidp;

	DBG(("n8_newsession: completed - session %d\n", *sidp));

	return 0;
}


/**********************************************************************
* FUNCTION:	n8_freesession(*arg, *sidp, *cri)
* DESCRIPTION:	The opencrypto free session handler.  This function
* 		frees the specified session, releasing any
* 		associated resources (e.g. N8 context).
* INPUTS:	arg	- N8 device softc
* 		tid	- The id of the session to free.
* RETURNS:	0 	- ok
* 		else an errno value
* NOTES:	XXX can this be called while the session is active?
**********************************************************************/
static int
n8_freesession(void *arg, u_int64_t tid)
{
	struct nsp_softc *sc = arg;
	uint32_t sid = ((uint32_t) tid) & 0xffffffff;
	nsp_session_t *session;

	if (sc == NULL) {
	    DBG(("n8_freesession(): sc == NULL\n"));
	    return EINVAL;
	}
	if (sid >= NSP_MAX_SESSION) {
		DBG(("n8_freesession(): sid %d out of range\n",sid));
		return EINVAL;
	}

	session = &sc->session[sid];

	/*
	 * Need to check if a partial operation is in progress
	 * and end it if so.
	 */
	if (session->active) {
		printf("n8_freesession: error - session 0x%04x is active.  Not freeing.\n", sid);
		//session->freePending = 1;
		/* should be protected by the framework.  If the session is 
		 * freed while active nasty things will happen for all of the data
		 * references.
		 */
		return 0;
	}

	n8_session_free(sc, session);

	DBG(("n8_freesession: completed - session %d\n", sid));
	return 0;
}


/**********************************************************************
* FUNCTION:	n8_process(*arg, *crp, hint)
* DESCRIPTION:	The opencrypto crypto operation handler.  This function
* 		expects to be called when the user app requests a
* 		cipher or hash operation for a crypto session.
* 		It kicks off the appopriate N8 async operation 
* 		to handle the request.
* INPUTS:	arg	- N8 device softc
* 		krp	- opencrypto crypto operation description
* 		hint	- not used.
* RETURNS:	0 	- ok
* 		else an errno value
**********************************************************************/
static int
n8_process(void *arg, struct cryptop *crp, int hint)
{
	struct nsp_softc *sc = arg;
	struct cryptodesc *crd;
	nsp_session_t *session;
	uint32_t sid = (uint32_t)(crp->crp_sid & 0xFFFFFFFF);
	int res=0;
	int hash_seen, crypto_seen;	/* booleans for error checking */
	int crd_count=0;		/* number of descriptors */

	DBG(("n8_process: session %d\n", sid));

	if (sc == NULL) {
	    DBG(("n8_process: sc == NULL\n"));
	    return EINVAL;
	}
	if (sid >= NSP_MAX_SESSION) {
		DBG(("n8_process: sid %d out of range\n",sid));
		return EINVAL;
	}
	session = &sc->session[sid];
	session->crp = crp;
	session->crd = NULL;

	DBG(("n8_process: crp_ilen %d crp_olen %d crp_mac %p\n", 
			crp->crp_ilen, crp->crp_olen, crp->crp_mac));

#ifdef NBDEBUG
	printf("n8_process: flags 0x%04x\n", crp->crp_flags);
	for (flag=1; flag<=0x80; flag<<=1) {
	    if (flag & crp->crp_flags) {
		switch (flag) {
		case CRYPTO_F_IMBUF:
			printf("\tCRYPTO_F_IMBUF\n");
			break;
		case CRYPTO_F_IOV:
			printf("\tCRYPTO_F_IOV\n");
			break;
		case CRYPTO_F_REL:
			printf("\tCRYPTO_F_REL\n");
			break;
		case CRYPTO_F_BATCH:
			printf("\tCRYPTO_F_BATCH\n");
			break;
		case CRYPTO_F_CBIMM:
			printf("\tCRYPTO_F_CBIMM\n");
			break;
		case CRYPTO_F_DONE:
			printf("\tCRYPTO_F_DONE\n");
			break;
		case CRYPTO_F_CBIFSYNC:
			printf("\tCRYPTO_F_CBIFSYNC\n");
			break;
		default:
			printf("\tUnknown flag 0x%x\n", flag);
			break;
		}
	    }
	}
#endif
	/* set up source and dest */
	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		session->src.mb = (struct mbuf *)crp->crp_buf;
		session->dst.mb = (struct mbuf *)crp->crp_buf;
		return EINVAL;	/* XXX to be done */
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
	    	session->src.io = (struct uio *)crp->crp_buf;
	    	session->dst.io = (struct uio *)crp->crp_buf;
		/* hash operations still use direct writes
		 * for the digest.
		 */
		session->mac = (N8_Buffer_t *)crp->crp_mac;
	} else {
		/* contiguous buffer in memory */
	    	session->src.ptr = (N8_Buffer_t *)crp->crp_buf;
	    	session->dst.ptr = (N8_Buffer_t *)crp->crp_buf;
		/* the crd will specify an injection offset for 
		 * the hash operation */
		session->mac = (N8_Buffer_t *)crp->crp_buf;
	}

	/* check data length */
	if (crp->crp_ilen > 0x4800) {
	    /* not yet supported - will require segmenting the data
	     * into multiple 18KB blocks, and multiple calls
	     * to the card, and probable copies of the input
	     * data to avoid overwriting the next block
	     * with ciphertext expansion.
	     */
	    printf("n8_process: crp_ilen %d too big\n", crp->crp_ilen);
	    return EFBIG;
	}
	if (crp->crp_olen > 0x4800) {
	    /* not yet supported - will require segmenting the data
	     * into multiple 18KB blocks, and multiple calls
	     * to the card, and probable copies of the input
	     * data to avoid overwriting the next block
	     * with ciphertext expansion.
	     */
	    printf("n8_process: crp_olen %d too big\n", crp->crp_olen);
	    return EFBIG;
	}

	/* check for supported set of operations */
	crypto_seen = 0;
	hash_seen = 0;
	for (crd=crp->crp_desc,crd_count=0; crd != NULL; crd=crd->crd_next, crd_count++) {
#ifdef N8DEBUG
		printf("n8_process: crd %d - crd_flags 0x%04x\n", crd_count, crd->crd_flags);
		if (crd->crd_flags & CRD_F_ENCRYPT)
			printf("\tCRD_F_ENCRYPT\n");
		if (crd->crd_flags & CRD_F_IV_PRESENT)
			printf("\tCRD_F_IV_PRESENT\n");
		if (crd->crd_flags & CRD_F_IV_EXPLICIT)
			printf("\tCRD_F_IV_EXPLICIT\n");
		if (crd->crd_flags & CRD_F_DSA_SHA_NEEDED)
			printf("\tCRD_F_DSA_SHA_NEEDED	\n");
#endif

		switch (crd->crd_alg) {
		case CRYPTO_MD5:
		case CRYPTO_SHA1:
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
			if (hash_seen) {
			    printf("n8_newsession(): multiple hash requests\n");
			    return EINVAL;
			}
			hash_seen = 1;
			break;

		case CRYPTO_AES_CBC:
			printf("n8_newsession(): invalid crypto op %d\n", crd->crd_alg);
			return EINVAL;
			break;

		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
			if (crypto_seen) {
			    printf("n8_newsession(): multiple crypto requests\n");
			    return EINVAL;
			}
			crypto_seen = 1;
			break;

		default:
			return EINVAL;
		}
	}

	/* kick off first operation */
	session->active = 1;
	res = n8_start_crd(sc, crp, crp->crp_desc, 0, session);

	if (res != 0) {
		/* op did not start, so session is not active */
		/* Note that the operation can complete before we
		 * get to here, hence the session is flagged as
		 * active before its started just in case.
		 */
		session->active = 0;
	}

	return res;
}	


/**********************************************************************
* FUNCTION:	n8_callback(*arg, status)
* DESCRIPTION:	Called when a crypto or hash operation is completed by
* 		the N8.
* 		This function checks for any further operations
* 		for the session and kicks the next one off.
* 		If all operations have completed it returns
* 		the results to the opencrypto framework.
* INPUTS:	arg	- the session context
* 		status	- The result of the N8 operation.
*
* RETURNS:	none.
**********************************************************************/
static void
n8_callback(void *arg, N8_Status_t status)
{
    	nsp_session_t *session=arg;
	struct cryptop *crp;
	int res;

	crp = session->crp;
	DBG(("n8_callback: session %d(0x%x) crd %d done, status %d\n",
			(uint32_t)(crp->crp_sid & 0xFFFFFFFF),
			session->magic,
			session->crd_id,
			status));

	if (status != N8_STATUS_OK) {
		DBG(("n8_callback: operation (crd_alg %d) failed (res=%d)\n",
					session->crd->crd_alg, status));
		crp->crp_etype = n8_map_errno(status);
		session->active = 0;
		crypto_done(crp);
		return;
	}

	/* Any more operations needed for this request? */
	if (session->crd->crd_next != NULL) {
		DBG(("n8_callback: starting next crd\n"));
		res = n8_start_crd(session->sc, session->crp,
				session->crd->crd_next, 
				session->crd_id + 1, 
				session);
		if (res != 0) {
			/* how do we let the user know? */
			DBG(("n8_callback: error %d starting crd\n", res));
			crp->crp_etype = res;
			session->active = 0;
			crypto_done(crp);
		}
	} else {
		/* no more ops so this request is complete */
		DBG(("n8_callback: crypto_done, session %d -> inactive\n",
			(session - session->sc->session)));
		session->active = 0;
		crypto_done(crp);
	}
}


/**********************************************************************
* FUNCTION:	n8_start_crd(*sc, *crp, *crd, crd_id, *session)
* DESCRIPTION:	Start a crypto operation.
* 		Kicks off a asynchronous crypto, or hash operation.
* INPUTS:	sc	- device data
* 		crp	- crypto operation request
* 		crd	- crypto operation to start
* 		crd_id	- opencrypto crypto session id
* 		session	- session record to manage the operation.
*
* RETURNS:	0 = success, else ERRNO value.
**********************************************************************/
static int
n8_start_crd(struct nsp_softc *sc,
	struct cryptop *crp,
	struct cryptodesc *crd,
	int crd_id,
	nsp_session_t *session)
{
	int res = 0;

#ifdef N8DEBUG
	printf("n8_start_crd: starting crd %d\n", crd_id);
	printf("n8_start_crd: crd_skip %d, crd_len %d, crd_inject %d\n",
			crd->crd_skip, 
			crd->crd_len, 
			crd->crd_inject);
	if (crd->crd_flags & CRD_F_ENCRYPT)
		printf("\tCRD_F_ENCRYPT\n");
	if (crd->crd_flags & CRD_F_IV_PRESENT)
		printf("\tCRD_F_IV_PRESENT\n");
	if (crd->crd_flags & CRD_F_IV_EXPLICIT)
		printf("\tCRD_F_IV_EXPLICIT\n");
	if (crd->crd_flags & CRD_F_DSA_SHA_NEEDED)
		printf("\tCRD_F_DSA_SHA_NEEDED	\n");
#endif

	session->crd = crd;
	session->crd_id = crd_id;

	/* note the errors should never occur since the crd list is
	 * pre-checked for validity
	 */
	switch (crd->crd_alg) {
	case CRYPTO_MD5:
	case CRYPTO_SHA1:
	case CRYPTO_MD5_HMAC:
	case CRYPTO_SHA1_HMAC:
		res = n8_do_hash(sc, crp, crd, session);
		break;

	case CRYPTO_DES_CBC:
	case CRYPTO_3DES_CBC:
		res = n8_do_crypt(sc, crp, crd, session);
		break;

	default:
		printf("n8_start_crd: invalid crypto op %d\n", crd->crd_alg);
		return EINVAL;
	}

	return res;
}


/**********************************************************************
* FUNCTION:	n8_do_crypt(*sc, *crp, *crd, *session)
* DESCRIPTION:	Kick off an encrypt or decrypt operation for a session.
* 		A session has one or more operations to perform on the
* 		data.  This function starts the operation specified
* 		by the crd.
* INPUTS:	sc	- the device state
* 		crp	- the crypto control data for the operation
* 		crd	- the crypto operation to start
* 		session	- session and context info for crypto ops
*
* RETURNS:	0 - operation sucessfully started.
* 		else ERRNO value.
**********************************************************************/
static int
n8_do_crypt(struct nsp_softc *sc,
	struct cryptop *crp,
	struct cryptodesc *crd,
	nsp_session_t *session)
{
	N8_Event_t event;
	int res=0;
	int ivlen = 8;	/* DES, 3DES CBC */

	/* IV Explicitly Provided? */
	if (crd->crd_flags & CRD_F_IV_EXPLICIT) {
		/* yes: use it for the operation */
		memcpy(session->cipherInfo.key.keyDES.IV,
				crd->crd_iv, ivlen);
	} else {
		/* No: use the earlier setup one */
		memcpy(session->cipherInfo.key.keyDES.IV,
				session->iv, ivlen);
	}

	/* IV not in place? */
	if ((crd->crd_flags & CRD_F_IV_PRESENT) == 0) {
		if (crp->crp_flags & CRYPTO_F_IMBUF) {
			m_copyback(session->src.mb, crd->crd_inject,
				ivlen, 
				session->cipherInfo.key.keyDES.IV);
		} else if (crp->crp_flags & CRYPTO_F_IOV) {
			cuio_copyback(session->src.io, crd->crd_inject,
				ivlen, 
				session->cipherInfo.key.keyDES.IV);
		}
	}

	res = N8_EncryptInitialize(&session->crypt, &session->contextHandle,
		    N8_CIPHER_DES, &session->cipherInfo, NULL);
	if (res != N8_STATUS_OK) {
		printf("N8_EncryptInitialize: Failed - res=%d\n", res);
		return n8_map_errno(res);
	}

	event.usrCallback = n8_callback;
	event.usrData = (void *)session;
	if (crd->crd_flags & CRD_F_ENCRYPT) {
	    if (crp->crp_flags & CRYPTO_F_IOV) {
		res = N8_Encrypt_uio(&session->crypt, session->src.io,
			crd->crd_len, session->dst.io, &event);
	    } else if (crp->crp_flags & CRYPTO_F_IMBUF) {
		printf("CRYPTO_F_IMBUF not implemented for Encrypt\n");
	    } else {
		res = N8_Encrypt(&session->crypt,
			session->src.ptr+crd->crd_skip,
			crd->crd_len, session->dst.ptr+crd->crd_inject, &event);
	    }
	} else {
	    if (crp->crp_flags & CRYPTO_F_IOV) {
		res = N8_Decrypt_uio(&session->crypt, session->src.io,
			crd->crd_len, session->dst.io, &event);
	    } else if (crp->crp_flags & CRYPTO_F_IMBUF) {
		printf("CRYPTO_F_IMBUF not implemented for Decrypt\n");
	    } else {
		res = N8_Decrypt(&session->crypt, session->src.ptr+crd->crd_skip,
			crd->crd_len, session->dst.ptr+crd->crd_inject, &event);
	    }
	}

	return n8_map_errno(res);
}


/**********************************************************************
* FUNCTION:	n8_do_hash(*sc, *crp, *crd, *session)
* DESCRIPTION:	Kick off a hash operation for a session.
* 		A session has one or more operations to perform on the
* 		data.  This function starts the operation specified
* 		by the crd.
* INPUTS:	sc	- the device state
* 		crp	- the crypto control data for the operation
* 		crd	- the crypto hash operation to start
* 		session	- session and context info for crypto ops
*
* RETURNS:	0 - operation sucessfully started.
* 		else ERRNO value.
**********************************************************************/
static int
n8_do_hash(struct nsp_softc *sc,
	struct cryptop *crp,
	struct cryptodesc *crd,
	nsp_session_t *session)
{
	N8_Event_t event;
	N8_HashInfo_t info;
	N8_HashAlgorithm_t  alg;
	int res=0;

	info.unitID = N8_ANY_UNIT;
	info.keyLength = 0;

	switch (crd->crd_alg) {
	case CRYPTO_MD5:
		alg = N8_MD5;
		break;
	case CRYPTO_SHA1:
		alg = N8_SHA1;
		break;
	case CRYPTO_MD5_HMAC:
		alg = N8_HMAC_MD5;
		if (crd->crd_klen == 0) {
			DBG(("CRYPTO_MD5_HMAC: using session key, len %d\n",session->mackeylen));
			/* use the session key */
			info.keyLength = session->mackeylen;
			info.key_p = session->mackey;

			/* Sanity check */
			if (session->mackeylen == 0) {
				printf("No key provided for HMAC\n");
				return EINVAL;
			}
		} else {
			info.keyLength = (crd->crd_klen+7)/8;
			info.key_p = crd->crd_key;
			DBG(("CRYPTO_MD5_HMAC: using crd key, len %d\n", info.keyLength));
		}
		break;
	case CRYPTO_SHA1_HMAC:
		alg = N8_HMAC_SHA1;
		if (crd->crd_klen == 0) {
			DBG(("CRYPTO_SHA1_HMAC: using session key, len %d\n",session->mackeylen));
			/* use the session key */
			info.keyLength = session->mackeylen;
			info.key_p = session->mackey;

			/* Sanity check */
			if (session->mackeylen == 0) {
				printf("No key provided for HMAC\n");
				return EINVAL;
			}
		} else {
			info.keyLength = (crd->crd_klen+7)/8;
			info.key_p = crd->crd_key;
			DBG(("CRYPTO_SHA1_HMAC: using crd key, len %d\n", info.keyLength));
		}
		break;
	case CRYPTO_RIPEMD160_HMAC:
		return ENODEV;
		break;
	default:
		printf("n8_do_hash: invalid hash algorithm %d\n", 
				crd->crd_alg);
		return ENODEV;
	}

	res = N8_HashInitialize(&session->hash, alg, &info, NULL);
	if (res != N8_STATUS_OK) {
		printf("n8_do_hash: N8_HashInitialize failed, error=%d\n",
				res);
		return n8_map_errno(res);
	}

	if ((crp->crp_flags & (CRYPTO_F_IOV | CRYPTO_F_IMBUF)) == 0) {
		/* use the crd's injection offset to obtain the output
		 * address for the digest.
		 */
		session->mac = session->src.ptr + crd->crd_inject;
	}

	DBG(("N8_HashInitialize: res=%d (crd_len=%d)\n", res, crd->crd_len));
	event.usrCallback = n8_callback;
	event.usrData = (void *)session;
	res = N8_HashCompleteMessage_uio(&session->hash, session->src.io,
			crd->crd_len, session->mac, &event);
	DBG(("N8_HashCompleteMessage_uio: res=%d\n", res));
	return n8_map_errno(res);
}


/**********************************************************************
* FUNCTION:	bn_le_to_be(*le, numbits, *be)
* DESCRIPTION:	Return the big-endian representation of the 
* 		little-endian byte stream.
* 		Basically a conversion from the opencrypto big-number format
* 		to the N8 big-number format.
* INPUTS:	le	- pointer to the little-endian byte-stream
* 		numbits	- number of bits to convert (this is rounded up
* 			  to be a multiple of 8).
* OUTPUTS: 	be	- big-endian byte stream conversion of *le
* RETURNS:	size of *be as number of bytes.
* NOTE:		assumes any remainder bits are zero, which is 
* 		true for OpenSSL bignum bit counts.
**********************************************************************/
static uint32_t
bn_le_to_be(uint8_t *le, int numbits, N8_Buffer_t *be)
{
	int numbytes;
	int ind;

	numbytes = (numbits+7)/8;

	for (ind=0; ind<numbytes; ind++) {
		be[ind] = le[numbytes-ind-1];
	}

	return numbytes;
}

/**********************************************************************
* FUNCTION:	n8_kcallback_finish(void *arg, N8_Status_t status)
* DESCRIPTION:	Interrupt handler for N8 key operation completion.
* 		This handler is called when the final N8 operation 
* 		completes for an asymmetric operation or modular
* 		arithmetic operation.
* 		If the status is ok, the results of the operation
* 		are passed back to the application vi opencrypto.
* 		Any bignum results are converted from big-endian
* 		to little-endian before being passed back.
*
* 		If the status was an error then it is noted for later.
* 		This handler will be invoked a second time by the N8 once 
* 		the operation has been cleaned up (status will be ok), 
* 		at which time the error is passed back to the application.
* INPUTS:	arg	- pointer to the key operation request data
* 			  to be a multiple of 8).
* 		status	- result of the operation.
* RETURNS:	none.
**********************************************************************/
static void
n8_kcallback_finish(void *arg, N8_Status_t status)
{
    	n8_kreq_t *req=arg;
	uint8_t data;

	DBG(("%s: status %d, req magic 0x%x\n", __func__, status, req->magic));

	if (req->krp == NULL) {
		printf("%s: called with NULL krp\n", __func__);
		return;
	}

	if (status == N8_STATUS_OK) {
		int ind;
		int parm;
		int res;

		/* was there an earlier error? */
		if (req->error) {
			/* Yes - we're done then. */
			printf("%s: finishing op, error was %d\n", __func__, req->error);
			if (req->krp->krp_op == CRK_DSA_VERIFY) {
				/* the CRK_DSA_VERIFY result semantics
				 * return 1 for verify ok, 0 for verify failed,
				 * and -1 for any other errors
				 */
				req->krp->krp_status = -1;
			} else {
				/* the rest of the operations return an ERRNO */
				req->krp->krp_status = req->error;	/* XXX translate to errno */
			}

			/* clean up N8 operations */
			switch (req->krp->krp_op) {
			case CRK_DSA_VERIFY:
			case CRK_DSA_SIGN:
				N8_DSAFreeKey(&req->op.dsa.key);
				break;
			case CRK_DH_COMPUTE_KEY:
				N8_DHFreeKey(&req->op.dh.key);
				break;
			case CRK_MOD_EXP_CRT:
				N8_RSAFreeKey(&req->op.rsa.key);
				break;
			default:
				/* nothing to clean up */
				break;
			}


			crypto_kdone(req->krp);
			req->krp = NULL;
			free(req, M_DEVBUF);	/* XXX use a pool... */
			return;
		}

		/* convert any bignum results to little-endian */
		for (parm=crk_def[req->krp->krp_op].iparmcount;
			       	parm < (crk_def[req->krp->krp_op].iparmcount + 
				       crk_def[req->krp->krp_op].oparmcount);
			       	parm++) {

			int numbytes = req->parm[parm].lengthBytes;

			/* need to convert bignum to little-endian? */
			if (crk_def[req->krp->krp_op].bignums & BN_ARG(parm)) {

				/* convert the big-endian result to little-endian */
				for (ind=0; ind<numbytes/2; ind++) {
					data = req->parm[parm].value_p[ind];
					req->parm[parm].value_p[ind] = req->parm[parm].value_p[numbytes-ind-1];
					req->parm[parm].value_p[numbytes-ind-1] = data;
				}
			}
			req->krp->krp_param[parm].crp_nbits = numbytes*8;
		}

		/* Deal with any special results and cleanups */
		switch (req->krp->krp_op) {
		case CRK_DSA_VERIFY:
			DBG(("CRK_DSA_VERIFY: result=%d\n", req->op.dsa.verifyok));
			/* return 0 for verify ok, 1 for verify failed */
			req->krp->krp_status = req->op.dsa.verifyok ? 0 : 1;
			res = N8_DSAFreeKey(&req->op.dsa.key);
			if (res != N8_STATUS_OK) {
				printf("%s: N8_DSAFreeKey failed, error=%d\n",
						__func__, res);
			}
			break;

		case CRK_DSA_SIGN:
			res = N8_DSAFreeKey(&req->op.dsa.key);
			if (res != N8_STATUS_OK) {
				printf("%s: N8_DSAFreeKey failed, error=%d\n",
						__func__, res);
			}
			req->krp->krp_status = 0;
			break;

		case CRK_DH_COMPUTE_KEY:
			res = N8_DHFreeKey(&req->op.dh.key);
			if (res != N8_STATUS_OK) {
				printf("%s: N8_DHFreeKey failed, error=%d\n",
						__func__, res);
			}
			req->krp->krp_status = 0;
			break;

		case CRK_MOD_EXP_CRT:
			res = N8_RSAFreeKey(&req->op.rsa.key);
			if (res != N8_STATUS_OK) {
				printf("%s: N8_RSAFreeKey failed, error=%d\n",
						__func__, res);
			}
			req->krp->krp_status = 0;
			break;

		default:
			req->krp->krp_status = 0;
			break;
		}

		/* Done - let the user app know */
		crypto_kdone(req->krp);
		req->krp = NULL;
		free(req, M_DEVBUF);	/* XXX replace with pool */
	} else {
		printf("%s: op failed, status=%d\n", __func__, status);
		req->krp->krp_status = n8_map_errno(status);
		req->error = status;
	}
}


/**********************************************************************
* FUNCTION:	n8_kcallback_setup(void *arg, N8_Status_t status)
* DESCRIPTION:	Interrupt handler for N8 key operation setup.
* 		This handler is called when the N8 key initialization
* 		operation completes.  If the initialization was 
* 		successful then the final N8 operation is started
* 		with the n8_kcallback_finish() handler set to be called
* 		on completion.
*
* 		If the status was an error then it is noted for later.
* 		This handler will be invoked a second time by the N8 once 
* 		the operation has been cleaned up (status will be ok), 
* 		at which time the error is passed back to the application.
* INPUTS:	arg	- pointer to the key operation request data
* 			  to be a multiple of 8).
* 		status	- result of the operation.
* RETURNS:	none.
**********************************************************************/
static void
n8_kcallback_setup(void *arg, N8_Status_t status)
{
    	n8_kreq_t *req=arg;
	N8_Event_t event;
	int res;

	DBG(("%s status %d\n", __func__, status));

	if (req->krp == NULL) {
		printf("%s called with NULL krp\n", __func__);
		return;
	}

	if (status == N8_STATUS_OK) {
		/* was there an earlier error? */
		if (req->error) {
			/* Yes - we're done then. */
			printf("%s: finishing op, error was %d\n", __func__, req->error);
			if (req->krp->krp_op == CRK_DSA_VERIFY) {
				/* the CRK_DSA_VERIFY result semantics
				 * return 1 for verify ok, 0 for verify failed,
				 * and -1 for any other errors
				 */
				req->krp->krp_status = -1;
			} else {
				/* the rest of the operations return an ERRNO */
				req->krp->krp_status = n8_map_errno(req->error);
			}
			crypto_kdone(req->krp);
			req->krp = NULL;
			free(req, M_DEVBUF);	/* use a pool... */
			return;
		}

		/* kick off the finishing op */
		event.usrCallback = n8_kcallback_finish;
		event.usrData = (void *)req;
		switch (req->krp->krp_op) {
		case CRK_DSA_SIGN:
			res = N8_DSASign(&req->op.dsa.key,
					req->parm[NSP_DSA_SIGN_DIGEST].value_p,
					req->parm[NSP_DSA_SIGN_RVALUE].value_p,
					req->parm[NSP_DSA_SIGN_SVALUE].value_p,
					&event);
			if (res != N8_STATUS_OK) {
				printf("%s: N8_DSASign failed, error = %d\n", __func__, res);
				req->krp->krp_status = n8_map_errno(res);
				crypto_kdone(req->krp);
				req->krp = NULL;
				free(req, M_DEVBUF);	/* XXX use a pool... */
			}
			break;

		case CRK_DSA_VERIFY:
			res = N8_DSAVerify(&req->op.dsa.key,
					req->parm[NSP_DSA_VERIFY_DIGEST].value_p,
					req->parm[NSP_DSA_VERIFY_RVALUE].value_p,
					req->parm[NSP_DSA_VERIFY_SVALUE].value_p,
					&req->op.dsa.verifyok,
					&event);
			if (res != N8_STATUS_OK) {
				printf("%s: N8_DSAVerify failed, error = %d\n", __func__, res);
				req->krp->krp_status = n8_map_errno(res);
				crypto_kdone(req->krp);
				req->krp = NULL;
				free(req, M_DEVBUF);	/* XXX use a pool... */
			}
			break;

		case CRK_DH_COMPUTE_KEY:
			DBG(("N8_DHCompute: PRIV=%08x %08x\n",
				((uint32_t *)req->parm[NSP_DH_COMPUTE_KEY_PRIV].value_p)[0],
				((uint32_t *)req->parm[NSP_DH_COMPUTE_KEY_PRIV].value_p)[1]));
			/* KEY = B^a % p, a and p already in dh.key */
			res = N8_DHCompute(&req->op.dh.key,
					NULL,	/* already have PUB key from init */
					req->parm[NSP_DH_COMPUTE_KEY_PRIV].value_p,
					req->parm[NSP_DH_COMPUTE_KEY_K].value_p,
					&event);
			if (res != N8_STATUS_OK) {
				printf("%s: N8_DHCompute failed, error = %d\n", __func__, res);
				req->krp->krp_status = n8_map_errno(res);
				crypto_kdone(req->krp);
				req->krp = NULL;
				free(req, M_DEVBUF);	/* XXX use a pool... */
			}
			break;

		default: /* coding error - shouldn't reach here */
			printf("%s: got unexpected krp_op %d\n", __func__, req->krp->krp_op);
			req->krp->krp_status = ENODEV;
			crypto_kdone(req->krp);
			req->krp = NULL;
			free(req, M_DEVBUF);	/* XXX use a pool... */
		}
	} else {
		printf("n8_kcallback: op failed, status=%d\n", status);
		req->krp->krp_status = status;	/* XXX translate to errno */
		req->error = status;
	}
}


/**********************************************************************
* FUNCTION:	check_key_parms(*krp)
* DESCRIPTION:	Check that each of the bignum parameters supplied by the
* 		key operation are valid for the operation.
* 		i.e. the number of input and output parameters is correct,
* 		and all of the bignum parameters are with the appropraite
* 		size range.
* INPUTS:	krp	- opencrypto key operation description
* RETURNS:	0 	- ok
* 		ENODEV	- invalid key operation
* 		EINVAL	- invalid number of input or output parameters
* 		ERANGE	- one or more parameters are outside
* 			  the range supported for big-numbers.
**********************************************************************/
static int
check_key_parms(struct cryptkop *krp)
{
	int ind;

	if (krp->krp_op > CRK_ALGORITHM_MAX) {
		printf("nsp: invalid crypto key op %d\n", krp->krp_op);
		return ENODEV;
	}
	if (krp->krp_iparams != crk_def[krp->krp_op].iparmcount) {
		printf("nsp: %s input params %d != %d\n",
				crk_def[krp->krp_op].name,
				krp->krp_iparams,
				crk_def[krp->krp_op].iparmcount);
		return EINVAL;

	}
	if (krp->krp_oparams != crk_def[krp->krp_op].oparmcount) {
		printf("nsp: %s output params %d != %d\n",
				crk_def[krp->krp_op].name,
				krp->krp_oparams,
				crk_def[krp->krp_op].oparmcount);
		return EINVAL;
	}

	for (ind=0; ind<(krp->krp_iparams + krp->krp_oparams); ind++) {
		/* is the parameter a bignum? */
		if (crk_def[krp->krp_op].bignums & BN_ARG(ind)) {
			/* check its size */
			if (krp->krp_param[ind].crp_nbits > (NSP_MAX_KEYLEN*8)) {
				printf("n8_kprocess: %s - param %d too large (%d bits)\n",
						crk_def[krp->krp_op].name,
						ind,
						krp->krp_param[ind].crp_nbits);
				return ERANGE;
			}
		}
	}

	return 0;
}


/**********************************************************************
* FUNCTION:	n8_kprocess(*arg, *krp, hint)
* DESCRIPTION:	The opencrypto key operation handler.  This function
* 		expects to be called when the user app requests a
* 		key operation.
* 		It kicks off the appopriate N8 async operation 
* 		to handle the request.
* INPUTS:	arg	- N8 device softc
* 		krp	- opencrypto key operation description
* 		hint	- not used.
* RETURNS:	0 	- ok
* 		else an errno value
**********************************************************************/
static int
n8_kprocess(void *arg, struct cryptkop *krp, int hint)
{
	struct nsp_softc *sc;

	N8_Event_t event;
	n8_kreq_t *req;			/* to hold result data */
	int res=0;
	int ind;

	sc = arg;

	if ((krp == NULL) || (krp->krp_callback == NULL))
		return (EINVAL);

	if (sc == NULL) {
		printf("n8_kprocess: error - sc == NULL\n");
		return EINVAL;
	}

	DBG(("n8_kprocess op %d, iparams %d, oparams %d, hid 0x%x\n",
			krp->krp_op,
			krp->krp_iparams,
			krp->krp_oparams,
			krp->krp_hid));

	/* Q. Does the final byte need to be masked?
	 * Q. Does the number need to be re-aligned?
	 */

	/* opencrypto bignum format:
	 * possibly a little-endian byte stream.
	 *
	 * N8 bignum format:
	 * 	an array of 256 words by 128 bits per operand
	 *
	 * N8 api bignum format:
	 * 	a network byte order string (big-endian).
	 *
	 *
	 * OpenSSL bignum:
	 * 	an array n of integers, with element n[0]
	 * 	containing the least-significant word of
	 * 	the bignum (word size is BN_BITS2 bits,
	 * 	which is 64, 32, 16, or 8 depending on machine
	 * 	architecture size of ulong).
	 * 	The elements are in little-endian byte order.
	 * 	The bignum includes a boolean to flag the number
	 * 	as negative (bn.neg).
	 *
	 *
	 * So, conversion from little-endian OpenSSL bignum
	 * to N8 api bignum requires the OpenSSL elements to be
	 * reversed, and the bytes in each element to be swapped.
	 *
	 * opencrypto BN -> N8 api BN:
	 * 	reverse the byte stream.
	 * 	If the number of bits is not a multiple of 8,
	 * 	will need to shift the final byte and pad with
	 * 	0 bits.
	 */
	res = check_key_parms(krp);
	if (res != 0) {
		krp->krp_status = res;
		crypto_kdone(krp);
		return 0;
	}

	/* setup a buffer to track this request and provide the callback handler
	 * with what it needs to complete it.
	 */
	req = (n8_kreq_t *)malloc(sizeof(n8_kreq_t), M_DEVBUF, M_NOWAIT);
	if (req == NULL) {
		printf("n8_kprocess: failed to alloc req (size=%d)\n", sizeof(n8_kreq_t));
		krp->krp_status = ENOMEM;
		crypto_kdone(krp);
		return 0;
	}
	req->magic = 0xFEEDFEED;

	/* convert input arguments from little-endian to big-endian */
	/* what about has values (e.g. DSA digests, etc) */
	DBG(("op %s, %d input %d output, bignums 0x%02x\n", 
			crk_def[krp->krp_op].name,
			crk_def[krp->krp_op].iparmcount,
			crk_def[krp->krp_op].oparmcount,
			crk_def[krp->krp_op].bignums));
	for (ind=0; ind<krp->krp_iparams; ind++) {
		req->parm[ind].value_p = &req->value[ind][0];
		if (crk_def[krp->krp_op].bignums & BN_ARG(ind)) {
			req->parm[ind].lengthBytes =
				bn_le_to_be(krp->krp_param[ind].crp_p,
				krp->krp_param[ind].crp_nbits,
				req->parm[ind].value_p);
		} else {
			/* unknown - defined by key algorithm */
			req->parm[ind].lengthBytes = 0;
		}
		DBG(("iparm %d length %d, ptr %p\n",
				ind, req->parm[ind].lengthBytes,
				req->parm[ind].value_p));
	}

	/* fill in output bignums */
	/* Results are converted to little-endian after the op completes. */
	for (ind=krp->krp_iparams;
			ind < (krp->krp_iparams + krp->krp_oparams);
			ind++) {
		req->parm[ind].value_p = krp->krp_param[ind].crp_p;
		req->parm[ind].lengthBytes = 
			(krp->krp_param[ind].crp_nbits+7)/8;
		DBG(("oparm %d bits -> %d bytes\n",
				krp->krp_param[ind].crp_nbits,
				req->parm[ind].lengthBytes));
		DBG(("oparm %d length %d, ptr %p\n",
				ind, req->parm[ind].lengthBytes,
				req->parm[ind].value_p));
	}

	/* this only supports a single result... DSA_SIGN needs two */
	req->error = N8_STATUS_OK;
	req->krp = krp;
	/* setup N8 call to call n8_kcallback with the request */

	switch (krp->krp_op) {
	case CRK_DSA_SIGN:
	        /* inputs: dgst dsa->p dsa->q dsa->g dsa->priv_key */
		event.usrCallback = n8_kcallback_setup;
		event.usrData = (void *)req;

		/*
		 * N8_DSAInitializeKey()
		 * 1) Put p in the parameter block.
		 * 2) Put q in the Parameter block.
		 * 3) Put privateKey in the parameter block.
		 * 4) Compute gR mod p and put it in the parameter block.
		 * 5) Compute cp = -(p[0]^-1  mod 2^128 and 
		 * put it in the parameter block.
		 */
		req->op.dsa.keymaterial.privateKey = req->parm[NSP_DSA_SIGN_X];
		req->op.dsa.keymaterial.p = req->parm[NSP_DSA_SIGN_P];
		req->op.dsa.keymaterial.q = req->parm[NSP_DSA_SIGN_Q];
		req->op.dsa.keymaterial.g  = req->parm[NSP_DSA_SIGN_G];
		req->op.dsa.keymaterial.unitID = N8_ANY_UNIT;
		res = N8_DSAInitializeKey(&req->op.dsa.key, N8_PRIVATE,
				&req->op.dsa.keymaterial, &event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("%s: N8_DSAInitializeKey failed, err=%d\n", __func__, res);
			krp->krp_status = res;
			crypto_kdone(krp);
			return 0;
		}
		break;

	case CRK_DSA_VERIFY:
		/* inputs: dgst dsa->p dsa->q dsa->g dsa->pub_key sig->r sig->s */
		event.usrCallback = n8_kcallback_setup;
		event.usrData = (void *)req;

		/*
		 * N8_DSAInitializeKey()
		 * 1) Put p in the parameter block.
		 * 2) Put q in the Parameter block.
		 * 3) Put privateKey in the parameter block.
		 * 4) Compute gR mod p and put it in the parameter block.
		 * 5) Compute cp = -(p[0]^-1  mod 2^128 and 
		 * put it in the parameter block.
		 */
		req->op.dsa.keymaterial.publicKey = req->parm[NSP_DSA_VERIFY_Y];
		req->op.dsa.keymaterial.p = req->parm[NSP_DSA_VERIFY_P];
		req->op.dsa.keymaterial.q = req->parm[NSP_DSA_VERIFY_Q];
		req->op.dsa.keymaterial.g  = req->parm[NSP_DSA_VERIFY_G];
		req->op.dsa.keymaterial.unitID = N8_ANY_UNIT;
		res = N8_DSAInitializeKey(&req->op.dsa.key, N8_PUBLIC,
				&req->op.dsa.keymaterial, &event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("%s: N8_DSAInitializeKey failed, err=%d\n", __func__, res);
			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;

	case CRK_DH_COMPUTE_KEY:
		{
		/* inputs: dh->priv_key pub_key dh->p (prime modulus) key output g^x */
		/* key = pub_key ^ priv_key % p,
		 * where pub_key is from second party, derived as pub_key = g^b % p, 
		 * where b is the second parties private key
		 */

		event.usrCallback = n8_kcallback_setup;
		event.usrData = (void *)req;

		req->op.dh.keymaterial.p =
		       	req->parm[NSP_DH_COMPUTE_KEY_P].value_p;
		req->op.dh.keymaterial.g =
		       	req->parm[NSP_DH_COMPUTE_KEY_PUB].value_p;
		req->op.dh.keymaterial.modulusSize = 
			req->parm[NSP_DH_COMPUTE_KEY_P].lengthBytes;
		req->op.dh.keymaterial.unitID = N8_ANY_UNIT;

		DBG(("N8_DHInitializeKey: P=%08x %08x, PUB=%08x %08x\n",
			((uint32_t *)req->parm[NSP_DH_COMPUTE_KEY_P].value_p)[0],
			((uint32_t *)req->parm[NSP_DH_COMPUTE_KEY_P].value_p)[1],
			((uint32_t *)req->parm[NSP_DH_COMPUTE_KEY_PUB].value_p)[0],
			((uint32_t *)req->parm[NSP_DH_COMPUTE_KEY_PUB].value_p)[1]));

		res = N8_DHInitializeKey(&req->op.dh.key,
				&req->op.dh.keymaterial, &event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("%s: N8_DHInitializeKey failed, err=%d\n", __func__, res);
			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;
	    }

	case CRK_MOD_EXP:
		/* inputs: a^p % m */
		event.usrCallback = n8_kcallback_finish;
		event.usrData = (void *)req;
		res = N8_ModExponentiate(
				&req->parm[NSP_MOD_EXP_A],
				&req->parm[NSP_MOD_EXP_B],
				&req->parm[NSP_MOD_EXP_M],
				&req->parm[NSP_MOD_EXP_R0],
				N8_ANY_UNIT,
				&event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("N8_ModExponentiate failed, err=%d\n",res);

			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;

	case CRK_MOD_EXP_CRT: 
		/* inputs: rsa->p rsa->q I rsa->dmp1 rsa->dmq1 rsa->iqmp */

		/* compute r0 = r0 ^ I mod rsa->n
		 * inputs from OpenSSL:rsa_mod_exp(r0, I, rsa)
		 * are p, q, I, dmp1 (DP), dmq1 (DQ), iqmp (QINV), r0.
		 *
		 * N8 requires at least N to do this operation,
		 * and N is not passed in by OpenSSL.
		 * Q. Can OpenSSL provide it?
		 * Q. Add another opencrypto op for this?
		 */

		/* Try doing this from a higher level so that N and D can
		 * be passed in?
		 * Construct the operation from smaller operations (slower)?
		 */

		krp->krp_status = EOPNOTSUPP;
		crypto_kdone(krp);
		break;

	case CRK_MOD_ADD:
		/* 
		 * inputs: A B Modulus
		 * result = (A + B) mod Modulus
		 */
		event.usrCallback = n8_kcallback_finish;
		event.usrData = (void *)req;
		res = N8_ModAdd(
				&req->parm[NSP_MOD_ADD_A],
				&req->parm[NSP_MOD_ADD_B],
				&req->parm[NSP_MOD_ADD_M],
				&req->parm[NSP_MOD_ADD_R0],
				N8_ANY_UNIT,
				&event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("N8_ModAdd failed, err=%d\n",res);

			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;

	case CRK_MOD_ADDINV:
		/* 
		 * inputs: A Modulus
		 * result = -A mod Modulus
		 */
		event.usrCallback = n8_kcallback_finish;
		event.usrData = (void *)req;
		res = N8_ModAdditiveInverse(
				&req->parm[NSP_MOD_ADDINV_A],
				&req->parm[NSP_MOD_ADDINV_M],
				&req->parm[NSP_MOD_ADDINV_R0],
				N8_ANY_UNIT,
				&event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("N8_ModAdditiveInverse failed, err=%d\n",res);

			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;

	case CRK_MOD_SUB:
		/* 
		 * inputs: A B Modulus
		 * result = (A - B) mod Modulus
		 */
		event.usrCallback = n8_kcallback_finish;
		event.usrData = (void *)req;
		res = N8_ModSubtract(
				&req->parm[NSP_MOD_SUB_A],
				&req->parm[NSP_MOD_SUB_B],
				&req->parm[NSP_MOD_SUB_M],
				&req->parm[NSP_MOD_SUB_R0],
				N8_ANY_UNIT,
				&event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("N8_ModSubtract failed, err=%d\n",res);

			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;

	case CRK_MOD_MULT:
		/* 
		 * inputs: A B Modulus
		 * result = (A * B) mod Modulus
		 */
		event.usrCallback = n8_kcallback_finish;
		event.usrData = (void *)req;
		res = N8_ModMultiply(
				&req->parm[NSP_MOD_MULT_A],
				&req->parm[NSP_MOD_MULT_B],
				&req->parm[NSP_MOD_MULT_M],
				&req->parm[NSP_MOD_MULT_R0],
				N8_ANY_UNIT,
				&event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("N8_ModMultiply failed, err=%d\n",res);

			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;

	case CRK_MOD_MULTINV:
		/* 
		 * inputs: A Modulus
		 * result = (A ^ -1) mod Modulus
		 */
		event.usrCallback = n8_kcallback_finish;
		event.usrData = (void *)req;
		res = N8_ModMultiplicativeInverse(
				&req->parm[NSP_MOD_MULTINV_A],
				&req->parm[NSP_MOD_MULTINV_M],
				&req->parm[NSP_MOD_MULTINV_R0],
				N8_ANY_UNIT,
				&event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("N8_ModMultiplicativeInverse failed, err=%d\n",res);

			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;

	case CRK_MOD:
		/* 
		 * inputs: A Modulus
		 * result = A mod Modulus
		 */
		event.usrCallback = n8_kcallback_finish;
		event.usrData = (void *)req;
		res = N8_Modulus(
				&req->parm[NSP_MODULUS_A],
				&req->parm[NSP_MODULUS_M],
				&req->parm[NSP_MODULUS_R0],
				N8_ANY_UNIT,
				&event);
		if (res != N8_STATUS_OK) {
			free(req, M_DEVBUF);
			printf("N8_Modulus failed, err=%d\n",res);

			krp->krp_status = n8_map_errno(res);
			crypto_kdone(krp);
			return 0;
		}
		break;

	default:
		printf("nsp: n8_kprocess: invalid op %d\n", krp->krp_op);
		krp->krp_status = EOPNOTSUPP;
		crypto_kdone(krp);
		break;
	}
	return res;
}

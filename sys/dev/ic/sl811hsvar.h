/*	$NetBSD: sl811hsvar.h,v 1.11 2013/10/02 22:55:04 skrll Exp $	*/

/*
 * Not (c) 2007 Matthew Orgass
 * This file is public domain, meaning anyone can make any use of part or all
 * of this file including copying into other works without credit.  Any use,
 * modified or not, is solely the responsibility of the user.  If this file is
 * part of a collection then use in the collection is governed by the terms of
 * the collection.
 */

/*
 * Cypress/ScanLogic SL811HS USB Host Controller
 */

#include <sys/gcq.h>

#define SC_DEV(sc)	((sc)->sc_dev)
#define SC_NAME(sc)	(device_xname(SC_DEV(sc)))

typedef unsigned int Frame;
struct slhci_pipe;

/* Generally transfer related items. */
struct slhci_transfers {
	struct usbd_xfer *rootintr;
	struct slhci_pipe *spipe[2]; 	/* current transfer (unless canceled) */
	struct gcq_head q[3];		/* transfer queues, Q_* index */
	struct gcq_head timed;		/* intr transfer multi-frame wait */
	struct gcq_head to;		/* timeout list */
	struct gcq_head ap;		/* all pipes */
	Frame frame;			/* current frame */
	unsigned int flags;		/* F_* flags */
	int pend;			/* pending for waitintr */
	int reserved_bustime;
	int16_t len[2];		     	/* length of transfer or -1 if none */
	uint8_t current_tregs[2][4]; 	/* ab, ADR, LEN, PID, DEV */
	uint8_t copyin[2]; 		/* copyin ADR, LEN */
	uint8_t rootaddr;		/* device address of root hub */
	uint8_t rootconf;		/* root configuration */
	uint8_t max_current;		/* max current / 2 */
	uint8_t sltype;			/* revision */
};

enum power_change {
	POWER_OFF,
	POWER_ON,
};

typedef void (*PowerFunc)(void *, enum power_change);

/* Attachment code must call slhci_preinit before registering the ISR */
struct slhci_softc {
	device_t		sc_dev;
	struct usbd_bus		sc_bus;

	kmutex_t		sc_lock;
	kmutex_t		sc_intr_lock;

	struct slhci_transfers	sc_transfers;	/* Info useful in transfers. */

	struct gcq_head		sc_waitq;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct callout		sc_timer; 	/* for reset */

	PowerFunc		sc_enable_power;

	device_t		sc_child;

	struct timeval		sc_reserved_warn_rate;
	struct timeval		sc_overflow_warn_rate;

	void			*sc_cb_softintr;

	unsigned int		sc_ier_check;

	int			sc_mem_use; /* XXX SLHCI_MEM_ACCOUNTING */

	uint8_t			sc_ier; 	/* enabled interrupts */
	uint32_t		sc_stride;	/* port stride */
};

/* last preinit arguments are: max current (in mA, not mA/2), port stride */
/* register access uses byte access, but stride offsets the data port */
int  slhci_supported_rev(uint8_t);
void slhci_preinit(struct slhci_softc *, PowerFunc, bus_space_tag_t,
    bus_space_handle_t, uint16_t, uint32_t);
int  slhci_attach(struct slhci_softc *);
int  slhci_detach(struct slhci_softc *, int);
int  slhci_activate(device_t, enum devact);
int  slhci_intr(void *);


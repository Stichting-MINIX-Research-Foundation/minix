/*	$NetBSD: motgvar.h,v 1.4 2014/09/13 18:36:39 jmcneill Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

#ifndef _MOTGVAR_H_
#define _MOTGVAR_H_

struct motg_pipe {
	struct usbd_pipe pipe;
	int nexttoggle;
	struct motg_hw_ep *hw_ep; /* pointer to the hardware EP used */
	SIMPLEQ_ENTRY(motg_pipe) ep_pipe_list; 
};

/* description of a hardware endpoint */
typedef enum {
	IDLE = 0,
	SETUP,
	DATA_IN,
	DATA_OUT,
	STATUS_IN,
	STATUS_OUT,
} usb_phase_t;

SIMPLEQ_HEAD(ep_pipes_head, motg_pipe);
struct motg_hw_ep {
	int ep_number;
	int ep_fifo_size;
	usbd_xfer_handle xfer;	/* active xfer on this EP */
	char *data; /* pointer to data to be transmitted/received */
	int datalen; /* data len to be transmitted */
	usb_phase_t phase; /* current phase of the transfer, if any */
	int refcount; /* how many devices using this EP */
	struct ep_pipes_head ep_pipes; /* list of pipes using this EP */
	bool need_short_xfer;
};
#define MOTG_MAX_HW_EP 16

struct motg_softc {
	device_t sc_dev;
	struct usbd_bus sc_bus;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	int sc_size;
	int sc_mode;
#define MOTG_MODE_HOST	0
#define MOTG_MODE_DEVICE 1
	void (*sc_intr_poll)(void *);
	void *sc_intr_poll_arg;
	int sc_ep_max;
	u_int sc_ep_fifosize;

	uint16_t sc_intr_tx_ep;
	uint16_t sc_intr_rx_ep;
	uint8_t  sc_intr_ctrl;

	struct motg_hw_ep sc_in_ep[MOTG_MAX_HW_EP];
	struct motg_hw_ep sc_out_ep[MOTG_MAX_HW_EP];

	struct usb_dma_reserve sc_dma_reserve;

	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;
	int sc_dying;

	pool_cache_t sc_xferpool;

	/* Info for the root hub interrupt "pipe". */
	usbd_xfer_handle sc_intr_xfer;	/* root hub interrupt transfer */
	uint8_t sc_root_addr;		/* address of the root hub */
	uint8_t sc_root_conf;		/* configuration of the root hub */

	char sc_vendor[32];		/* vendor string for root hub */
	int sc_id_vendor;		/* vendor ID for root hub */

	int	sc_port_enabled : 1;
	int	sc_port_enabled_changed : 1;
	int	sc_port_suspended : 1;
	int	sc_port_suspended_change : 1;
	int	sc_high_speed : 1;
	int	sc_connected : 1;
	int	sc_connected_changed : 1;
	int	sc_isreset : 1;

	device_t sc_child;		/* /dev/usb# device */
};

struct motg_xfer {
	struct usbd_xfer xfer;
	struct motg_softc *sc;
};

#define UXFER(xfer) ((struct motg_xfer *)(xfer))


usbd_status	motg_init(struct motg_softc *);
int		motg_intr(struct motg_softc *, uint16_t, uint16_t, uint8_t);
int		motg_intr_vbus(struct motg_softc *, int);
int		motg_detach(struct motg_softc *, int);
void		motg_childdet(device_t, device_t);
int		motg_activate(device_t, enum devact);
bool		motg_resume(device_t, const pmf_qual_t *);
bool		motg_suspend(device_t, const pmf_qual_t *);

#endif /* _MOTGVAR_H_ */

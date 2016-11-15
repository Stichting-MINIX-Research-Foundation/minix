/*	$NetBSD: rlvar.h,v 1.8 2008/03/11 05:34:02 matt Exp $	*/

/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * RL11/RLV11/RLV12 disk controller driver and
 * RL01/RL02 disk device driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rlvar.h,v 1.8 2008/03/11 05:34:02 matt Exp $");

struct rlc_softc {
	device_t sc_dev;
	struct uba_softc *sc_uh;
	struct evcnt sc_intrcnt;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmam;
	struct bufq_state *sc_q;	/* Queue of waiting bufs */
	struct buf *sc_active;		/* Currently active buf */
	void *sc_bufaddr;		/* Current in-core address */
	int sc_diskblk;			/* Current block on disk */
	int sc_bytecnt;			/* How much left to transfer */
};

struct rl_softc {
	device_t rc_dev;
	struct rlc_softc *rc_rlc;
	struct disk rc_disk;
	int rc_state;
	int rc_head;
	int rc_cyl;
	int rc_hwid;
};

struct rlc_attach_args {
	u_int16_t type;
	int hwid;
};


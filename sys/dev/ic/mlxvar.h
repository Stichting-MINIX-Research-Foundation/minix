/*	$NetBSD: mlxvar.h,v 1.15 2012/10/27 17:18:22 chs Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Copyright (c) 1999 Michael Smith
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
 * from FreeBSD: mlxvar.h,v 1.5.2.2 2000/04/24 19:40:50 msmith Exp
 */

#ifndef _IC_MLXVAR_H_
#define	_IC_MLXVAR_H_

/* Older boards allow up to 17 segments and 64kB transfers. */
#define	MLX_MAX_SEGS		17
#define	MLX_MAX_XFER		65536
#define MLX_SGL_SIZE		(sizeof(struct mlx_sgentry) * MLX_MAX_SEGS)

/* This shouldn't be ajusted lightly... */
#define MLX_MAX_DRIVES		32

/* Maximum queue depth, matching the older controllers. */
#define	MLX_MAX_QUEUECNT	63

/* Number of CCBs to reserve for control operations. */
#define	MLX_NCCBS_CONTROL	7

/* Structure describing a system drive as attached to the controller. */
struct mlx_sysdrive {
	u_int32_t	ms_size;
	u_short		ms_state;
	u_short		ms_raidlevel;
	device_t	ms_dv;
};

/* Optional per-CCB context. */
struct mlx_ccb;
struct mlx_context {
	void	(*mx_handler)(struct mlx_ccb *);
	void 	*mx_context;
	device_t	mx_dv;
};

/* Command control block. */
struct mlx_ccb {
	union {
		SIMPLEQ_ENTRY(mlx_ccb) simpleq;
		SLIST_ENTRY(mlx_ccb) slist;
		TAILQ_ENTRY(mlx_ccb) tailq;
	} mc_chain;
	u_int		mc_flags;
	u_int		mc_status;
	u_int		mc_ident;
	time_t		mc_expiry;
	u_int		mc_nsgent;
	u_int		mc_xfer_size;
	bus_addr_t	mc_xfer_phys;
	bus_dmamap_t	mc_xfer_map;
	struct mlx_context mc_mx;
	u_int8_t	mc_mbox[16];
};
#define	MC_XFER_IN	MU_XFER_IN	/* Map describes inbound xfer */
#define	MC_XFER_OUT	MU_XFER_OUT	/* Map describes outbound xfer */
#define	MC_WAITING	0x0400		/* We have waiters */
#define	MC_CONTROL	0x0800		/* Control operation */

/*
 * Per-controller state.
 */
struct mlx_softc {
	device_t		mlx_dv;
	bus_space_tag_t		mlx_iot;
	bus_space_handle_t	mlx_ioh;
	bus_dma_tag_t		mlx_dmat;
	bus_dmamap_t		mlx_dmamap;
	void			*mlx_ih;

	SLIST_HEAD(, mlx_ccb)	mlx_ccb_freelist;
	TAILQ_HEAD(, mlx_ccb)	mlx_ccb_worklist;
	SIMPLEQ_HEAD(, mlx_ccb)	mlx_ccb_queue;
	struct mlx_ccb		*mlx_ccbs;
	int			mlx_nccbs;
	int			mlx_nccbs_ctrl;

	void *			mlx_sgls;
	bus_addr_t		mlx_sgls_paddr;

	int	(*mlx_submit)(struct mlx_softc *, struct mlx_ccb *);
	int	(*mlx_findcomplete)(struct mlx_softc *, u_int *, u_int *);
	void	(*mlx_intaction)(struct mlx_softc *, int);
	int	(*mlx_fw_handshake)(struct mlx_softc *, int *, int *, int *);
	int	(*mlx_reset)(struct mlx_softc *);

	int			mlx_max_queuecnt;
	struct mlx_cinfo	mlx_ci;

	time_t			mlx_lastpoll;
	u_int			mlx_lastevent;
	u_int			mlx_currevent;
	u_int			mlx_bg;
	struct mlx_rebuild_status mlx_rebuildstat;
	struct mlx_pause	mlx_pause;
	int			mlx_flags;

	struct mlx_sysdrive	mlx_sysdrive[MLX_MAX_DRIVES];
	int			mlx_numsysdrives;
};

#define MLX_BG_CHECK		1	/* we started a check */
#define MLX_BG_REBUILD		2	/* we started a rebuild */
#define MLX_BG_SPONTANEOUS	3	/* it just happened somehow */

#define MLXF_SPINUP_REPORTED	0x0001	/* "spinning up drives" displayed */
#define MLXF_EVENTLOG_BUSY	0x0002	/* currently reading event log */
#define MLXF_FW_INITTED		0x0004	/* firmware init crap done */
#define MLXF_PAUSEWORKS		0x0008	/* channel pause works as expected */
#define MLXF_OPEN		0x0010	/* control device is open */
#define	MLXF_INITOK		0x0020	/* controller initialised OK */
#define	MLXF_PERIODIC_CTLR	0x0040	/* periodic check running */
#define	MLXF_PERIODIC_DRIVE	0x0080	/* periodic check running */
#define	MLXF_PERIODIC_REBUILD	0x0100	/* periodic check running */
#define	MLXF_RESCANNING		0x0400	/* rescanning drive table */

struct mlx_attach_args {
	int		mlxa_unit;
};

int	mlx_flush(struct mlx_softc *, int);
void	mlx_init(struct mlx_softc *, const char *);
int	mlx_intr(void *);

int	mlx_ccb_alloc(struct mlx_softc *, struct mlx_ccb **, int);
const char *mlx_ccb_diagnose(struct mlx_ccb *);
void	mlx_ccb_enqueue(struct mlx_softc *, struct mlx_ccb *);
void	mlx_ccb_free(struct mlx_softc *, struct mlx_ccb *);
int	mlx_ccb_map(struct mlx_softc *, struct mlx_ccb *, void *, int, int);
int	mlx_ccb_poll(struct mlx_softc *, struct mlx_ccb *, int);
void	mlx_ccb_unmap(struct mlx_softc *, struct mlx_ccb *);
int	mlx_ccb_wait(struct mlx_softc *, struct mlx_ccb *);

static __inline void	mlx_make_type1(struct mlx_ccb *, u_int8_t, u_int16_t,
				       u_int32_t, u_int8_t, u_int32_t,
				       u_int8_t);
static __inline void	mlx_make_type2(struct mlx_ccb *, u_int8_t, u_int8_t,
				       u_int8_t, u_int8_t, u_int8_t,
				       u_int8_t, u_int8_t, u_int32_t,
				       u_int8_t);
static __inline void	mlx_make_type3(struct mlx_ccb *, u_int8_t, u_int8_t,
				       u_int8_t, u_int16_t, u_int8_t,
				       u_int8_t, u_int32_t, u_int8_t);
static __inline void	mlx_make_type4(struct mlx_ccb *, u_int8_t, u_int16_t,
				       u_int32_t, u_int32_t, u_int8_t);
static __inline void	mlx_make_type5(struct mlx_ccb *, u_int8_t,  u_int8_t,
				       u_int8_t, u_int32_t, u_int32_t,
				       u_int8_t);

static __inline u_int8_t	mlx_inb(struct mlx_softc *, int);
static __inline u_int16_t	mlx_inw(struct mlx_softc *, int);
static __inline u_int32_t	mlx_inl(struct mlx_softc *, int);
static __inline void		mlx_outb(struct mlx_softc *, int, u_int8_t);
static __inline void		mlx_outw(struct mlx_softc *, int, u_int16_t);
static __inline void		mlx_outl(struct mlx_softc *, int, u_int32_t);

static __inline void
mlx_make_type1(struct mlx_ccb *mc, u_int8_t code, u_int16_t f1, u_int32_t f2,
	       u_int8_t f3, u_int32_t f4, u_int8_t f5)
{

	mc->mc_mbox[0x0] = code;
	mc->mc_mbox[0x2] = f1;
	mc->mc_mbox[0x3] = ((f2 >> 18) & 0xc0) | ((f1 >> 8) & 0x3f);
	mc->mc_mbox[0x4] = f2;
	mc->mc_mbox[0x5] = (f2 >> 8);
	mc->mc_mbox[0x6] = (f2 >> 16);
	mc->mc_mbox[0x7] = f3;
	mc->mc_mbox[0x8] = f4;
	mc->mc_mbox[0x9] = (f4 >> 8);
	mc->mc_mbox[0xa] = (f4 >> 16);
	mc->mc_mbox[0xb] = (f4 >> 24);
	mc->mc_mbox[0xc] = f5;
}

static __inline void
mlx_make_type2(struct mlx_ccb *mc, u_int8_t code, u_int8_t f1, u_int8_t f2,
	       u_int8_t f3, u_int8_t f4, u_int8_t f5, u_int8_t f6,
	       u_int32_t f7, u_int8_t f8)
{

	mc->mc_mbox[0x0] = code;
	mc->mc_mbox[0x2] = f1;
	mc->mc_mbox[0x3] = f2;
	mc->mc_mbox[0x4] = f3;
	mc->mc_mbox[0x5] = f4;
	mc->mc_mbox[0x6] = f5;
	mc->mc_mbox[0x7] = f6;
	mc->mc_mbox[0x8] = f7;
	mc->mc_mbox[0x9] = (f7 >> 8);
	mc->mc_mbox[0xa] = (f7 >> 16);
	mc->mc_mbox[0xb] = (f7 >> 24);
	mc->mc_mbox[0xc] = f8;
}

static __inline void
mlx_make_type3(struct mlx_ccb *mc, u_int8_t code, u_int8_t f1, u_int8_t f2,
	       u_int16_t f3, u_int8_t f4, u_int8_t f5, u_int32_t f6,
	       u_int8_t f7)
{

	mc->mc_mbox[0x0] = code;
	mc->mc_mbox[0x2] = f1;
	mc->mc_mbox[0x3] = f2;
	mc->mc_mbox[0x4] = f3;
	mc->mc_mbox[0x5] = (f3 >> 8);
	mc->mc_mbox[0x6] = f4;
	mc->mc_mbox[0x7] = f5;
	mc->mc_mbox[0x8] = f6;
	mc->mc_mbox[0x9] = (f6 >> 8);
	mc->mc_mbox[0xa] = (f6 >> 16);
	mc->mc_mbox[0xb] = (f6 >> 24);
	mc->mc_mbox[0xc] = f7;
}

static __inline void
mlx_make_type4(struct mlx_ccb *mc, u_int8_t code,  u_int16_t f1, u_int32_t f2,
	       u_int32_t f3, u_int8_t f4)
{

	mc->mc_mbox[0x0] = code;
	mc->mc_mbox[0x2] = f1;
	mc->mc_mbox[0x3] = (f1 >> 8);
	mc->mc_mbox[0x4] = f2;
	mc->mc_mbox[0x5] = (f2 >> 8);
	mc->mc_mbox[0x6] = (f2 >> 16);
	mc->mc_mbox[0x7] = (f2 >> 24);
	mc->mc_mbox[0x8] = f3;
	mc->mc_mbox[0x9] = (f3 >> 8);
	mc->mc_mbox[0xa] = (f3 >> 16);
	mc->mc_mbox[0xb] = (f3 >> 24);
	mc->mc_mbox[0xc] = f4;
}

static __inline void
mlx_make_type5(struct mlx_ccb *mc, u_int8_t code, u_int8_t f1, u_int8_t f2,
	       u_int32_t f3, u_int32_t f4, u_int8_t f5)
{

	mc->mc_mbox[0x0] = code;
	mc->mc_mbox[0x2] = f1;
	mc->mc_mbox[0x3] = f2;
	mc->mc_mbox[0x4] = f3;
	mc->mc_mbox[0x5] = (f3 >> 8);
	mc->mc_mbox[0x6] = (f3 >> 16);
	mc->mc_mbox[0x7] = (f3 >> 24);
	mc->mc_mbox[0x8] = f4;
	mc->mc_mbox[0x9] = (f4 >> 8);
	mc->mc_mbox[0xa] = (f4 >> 16);
	mc->mc_mbox[0xb] = (f4 >> 24);
	mc->mc_mbox[0xc] = f5;
}

static __inline u_int8_t
mlx_inb(struct mlx_softc *mlx, int off)
{

	bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh, off, 1,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_1(mlx->mlx_iot, mlx->mlx_ioh, off));
}

static __inline u_int16_t
mlx_inw(struct mlx_softc *mlx, int off)
{

	bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh, off, 2,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_2(mlx->mlx_iot, mlx->mlx_ioh, off));
}

static __inline u_int32_t
mlx_inl(struct mlx_softc *mlx, int off)
{

	bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh, off, 4,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(mlx->mlx_iot, mlx->mlx_ioh, off));
}

static __inline void
mlx_outb(struct mlx_softc *mlx, int off, u_int8_t val)
{

	bus_space_write_1(mlx->mlx_iot, mlx->mlx_ioh, off, val);
	bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh, off, 1,
	    BUS_SPACE_BARRIER_WRITE);
}

static __inline void
mlx_outw(struct mlx_softc *mlx, int off, u_int16_t val)
{

	bus_space_write_2(mlx->mlx_iot, mlx->mlx_ioh, off, val);
	bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh, off, 2,
	    BUS_SPACE_BARRIER_WRITE);
}

static __inline void
mlx_outl(struct mlx_softc *mlx, int off, u_int32_t val)
{

	bus_space_write_4(mlx->mlx_iot, mlx->mlx_ioh, off, val);
	bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh, off, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

#endif	/* !_IC_MLXVAR_H_ */

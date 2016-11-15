/*	$NetBSD: pcmciavar.h,v 1.35 2011/07/26 22:24:36 dyoung Exp $	*/

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/types.h>
#include <sys/queue.h>

#include <dev/pcmcia/pcmciachip.h>

extern int	pcmcia_verbose;

/*
 * Contains information about mapped/allocated i/o spaces.
 */
struct pcmcia_io_handle {
	bus_space_tag_t iot;		/* bus space tag (from chipset) */
	bus_space_handle_t ioh;		/* mapped space handle */
	bus_addr_t      addr;		/* resulting address in bus space */
	bus_size_t      size;		/* size of i/o space */
	int             flags;		/* misc. information */
	void		*ihandle;	/* opaque i/o handle */
};

#define	PCMCIA_IO_ALLOCATED	0x01	/* i/o space was allocated */

/*
 * Contains information about allocated memory space.
 */
struct pcmcia_mem_handle {
	bus_space_tag_t memt;		/* bus space tag (from chipset) */
	bus_space_handle_t memh;	/* mapped space handle */
	bus_addr_t      addr;		/* resulting address in bus space */
	bus_size_t      size;		/* size of mem space */
	pcmcia_mem_handle_t mhandle;	/* opaque memory handle */
	bus_size_t      realsize;	/* how much we really allocated */
};

/* pcmcia itself */

#define PCMCIA_CFE_MWAIT_REQUIRED	0x0001
#define PCMCIA_CFE_RDYBSY_ACTIVE	0x0002
#define PCMCIA_CFE_WP_ACTIVE		0x0004
#define PCMCIA_CFE_BVD_ACTIVE		0x0008
#define PCMCIA_CFE_IO8			0x0010
#define PCMCIA_CFE_IO16			0x0020
#define PCMCIA_CFE_IRQSHARE		0x0040
#define PCMCIA_CFE_IRQPULSE		0x0080
#define PCMCIA_CFE_IRQLEVEL		0x0100
#define PCMCIA_CFE_POWERDOWN		0x0200
#define PCMCIA_CFE_READONLY		0x0400
#define PCMCIA_CFE_AUDIO		0x0800

struct pcmcia_config_entry {
	int		number;
	u_int32_t	flags;
	int		iftype;
	int		num_iospace;

	/*
	 * The card will only decode this mask in any case, so we can
	 * do dynamic allocation with this in mind, in case the suggestions
	 * below are no good.
	 */
	u_long		iomask;
	struct {
		u_long	length;
		u_long	start;

		struct	pcmcia_io_handle handle;
		int	window;
	} iospace[4];		/* XXX this could be as high as 16 */
	u_int16_t	irqmask;
	int		num_memspace;
	struct {
		u_long	length;
		u_long	cardaddr;
		u_long	hostaddr;

		struct	pcmcia_mem_handle handle;
		bus_size_t offset;
		int	window;

	} memspace[2];		/* XXX this could be as high as 8 */
	int		maxtwins;
	SIMPLEQ_ENTRY(pcmcia_config_entry) cfe_list;
};


struct pcmcia_funce_disk {
	int pfd_interface;
};

struct pcmcia_funce_lan {
	int pfl_nidlen;
	u_int8_t pfl_nid[8];
};

union pcmcia_funce {
	struct pcmcia_funce_disk pfv_disk;
	struct pcmcia_funce_lan pfv_lan;
};


struct pcmcia_function {
	/* read off the card */
	int		number;
	int		function;
	int		last_config_index;
	u_long		ccr_base;
	u_long		ccr_mask;
	SIMPLEQ_HEAD(, pcmcia_config_entry) cfe_head;
	SIMPLEQ_ENTRY(pcmcia_function) pf_list;
	/* run-time state */
	struct pcmcia_softc *sc;
	device_t child;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_mem_handle pf_pcmh;
#define	pf_ccrt		pf_pcmh.memt
#define	pf_ccrh		pf_pcmh.memh
#define	pf_ccr_mhandle	pf_pcmh.mhandle
#define	pf_ccr_realsize	pf_pcmh.realsize
	bus_size_t	pf_ccr_offset;
	int		pf_ccr_window;
	bus_addr_t	pf_mfc_iobase;
	bus_addr_t	pf_mfc_iomax;
	void		*pf_ih;
	int		pf_flags;

	union pcmcia_funce pf_funce; /* CISTPL_FUNCE */
#define pf_funce_disk_interface pf_funce.pfv_disk.pfd_interface
#define pf_funce_lan_nid pf_funce.pfv_lan.pfl_nid
#define pf_funce_lan_nidlen pf_funce.pfv_lan.pfl_nidlen
};

/* pf_flags */
#define	PFF_ENABLED	0x0001		/* function is enabled */
#define	PFF_DETACHED	0x0002		/* card is detached */

SIMPLEQ_HEAD(pcmcia_function_head, pcmcia_function);

struct pcmcia_card {
	int		cis1_major;
	int		cis1_minor;
	/* XXX waste of space? */
	char		cis1_info_buf[256];
	char		*cis1_info[4];
	/*
	 * Use int32_t for manufacturer and product so that they can
	 * hold the id value found in card CIS and special value that
	 * indicates no id was found.
	 */
	int32_t		manufacturer;
#define	PCMCIA_VENDOR_INVALID	-1
	int32_t		product;
#define	PCMCIA_PRODUCT_INVALID		-1
	u_int16_t	error;
#define	PCMCIA_CIS_INVALID		{ NULL, NULL, NULL, NULL }
	struct pcmcia_function_head	pf_head;
};

struct pcmcia_softc {
	device_t	dev;

	/* this stuff is for the socket */
	pcmcia_chipset_tag_t pct;
	pcmcia_chipset_handle_t pch;

	/* this stuff is for the card */
	struct pcmcia_card card;
	void		*ih;
	int		sc_enabled_count;	/* how many functions are
						   enabled */
};

struct pcmcia_cis_quirk {
	int32_t manufacturer;
	int32_t product;
	const char *cis1_info[4];
	const struct pcmcia_function *pf;
	const struct pcmcia_config_entry *cfe;
};

struct pcmcia_attach_args {
	int32_t manufacturer;
	int32_t product;
	struct pcmcia_card *card;
	struct pcmcia_function *pf;
};

struct pcmcia_tuple {
	unsigned int	code;
	unsigned int	length;
	u_long		mult;
	bus_size_t	ptr;
	bus_space_tag_t	memt;
	bus_space_handle_t memh;
};

struct pcmcia_product {
	u_int32_t	pp_vendor;
	u_int32_t	pp_product;
	const char	*pp_cisinfo[4];
};

typedef int (*pcmcia_product_match_fn)(struct pcmcia_attach_args *,
    const struct pcmcia_product *, int);

const void *pcmcia_product_lookup(struct pcmcia_attach_args *, const void *,
	    size_t, size_t, pcmcia_product_match_fn);

void	pcmcia_devinfo(struct pcmcia_card *, int, char *, size_t);

void	pcmcia_read_cis(struct pcmcia_softc *);
void	pcmcia_check_cis_quirks(struct pcmcia_softc *);
void	pcmcia_print_cis(struct pcmcia_softc *);
int	pcmcia_scan_cis(device_t,
	    int (*) (struct pcmcia_tuple *, void *), void *);

#define	pcmcia_cis_read_1(tuple, idx0)					\
	(bus_space_read_1((tuple)->memt, (tuple)->memh, (tuple)->mult*(idx0)))

#define	pcmcia_tuple_read_1(tuple, idx1)				\
	(pcmcia_cis_read_1((tuple), ((tuple)->ptr+(2+(idx1)))))

#define	pcmcia_tuple_read_2(tuple, idx2)				\
	(pcmcia_tuple_read_1((tuple), (idx2)) | 			\
	 (pcmcia_tuple_read_1((tuple), (idx2)+1)<<8))

#define	pcmcia_tuple_read_3(tuple, idx3)				\
	(pcmcia_tuple_read_1((tuple), (idx3)) |				\
	 (pcmcia_tuple_read_1((tuple), (idx3)+1)<<8) |			\
	 (pcmcia_tuple_read_1((tuple), (idx3)+2)<<16))

#define	pcmcia_tuple_read_4(tuple, idx4)				\
	(pcmcia_tuple_read_1((tuple), (idx4)) |				\
	 (pcmcia_tuple_read_1((tuple), (idx4)+1)<<8) |			\
	 (pcmcia_tuple_read_1((tuple), (idx4)+2)<<16) |			\
	 (pcmcia_tuple_read_1((tuple), (idx4)+3)<<24))

#define	pcmcia_tuple_read_n(tuple, n, idxn)				\
	(((n)==1)?pcmcia_tuple_read_1((tuple), (idxn)) :		\
	 (((n)==2)?pcmcia_tuple_read_2((tuple), (idxn)) :		\
	  (((n)==3)?pcmcia_tuple_read_3((tuple), (idxn)) :		\
	   /* n == 4 */ pcmcia_tuple_read_4((tuple), (idxn)))))

#define	PCMCIA_SPACE_MEMORY	1
#define	PCMCIA_SPACE_IO		2

int	pcmcia_ccr_read(struct pcmcia_function *, int);
void	pcmcia_ccr_write(struct pcmcia_function *, int, int);

#define	pcmcia_mfc(sc)	(! SIMPLEQ_EMPTY(&(sc)->card.pf_head) &&	\
		 SIMPLEQ_NEXT(SIMPLEQ_FIRST(&(sc)->card.pf_head), pf_list))

void	pcmcia_socket_enable(device_t);
void	pcmcia_socket_disable(device_t);
void	pcmcia_socket_settype(device_t, int);

int	pcmcia_config_alloc(struct pcmcia_function *,
	    struct pcmcia_config_entry *);
void	pcmcia_config_free(struct pcmcia_function *);
int	pcmcia_config_map(struct pcmcia_function *);
void	pcmcia_config_unmap(struct pcmcia_function *);


int	pcmcia_function_configure(struct pcmcia_function *,
	    int (*validator)(struct pcmcia_config_entry *));
void	pcmcia_function_unconfigure(struct pcmcia_function *);
void	pcmcia_function_init(struct pcmcia_function *,
	    struct pcmcia_config_entry *);
int	pcmcia_function_enable(struct pcmcia_function *);
void	pcmcia_function_disable(struct pcmcia_function *);

#define	pcmcia_io_alloc(pf, start, size, align, pciop)			\
	(pcmcia_chip_io_alloc((pf)->sc->pct, pf->sc->pch, (start),	\
	 (size), (align), (pciop)))

#define	pcmcia_io_free(pf, pciohp)					\
	(pcmcia_chip_io_free((pf)->sc->pct, (pf)->sc->pch, (pciohp)))

int	pcmcia_io_map(struct pcmcia_function *, int,
	    struct pcmcia_io_handle *, int *);
void	pcmcia_io_unmap(struct pcmcia_function *, int);

void	pcmcia_free_pf(struct pcmcia_function_head *);

#define pcmcia_mem_alloc(pf, size, pcmhp)				\
	(pcmcia_chip_mem_alloc((pf)->sc->pct, (pf)->sc->pch, (size), (pcmhp)))

#define pcmcia_mem_free(pf, pcmhp)					\
	(pcmcia_chip_mem_free((pf)->sc->pct, (pf)->sc->pch, (pcmhp)))

#define pcmcia_mem_map(pf, kind, card_addr, size, pcmhp, offsetp, windowp) \
	(pcmcia_chip_mem_map((pf)->sc->pct, (pf)->sc->pch, (kind),	\
	 (card_addr), (size), (pcmhp), (offsetp), (windowp)))

#define	pcmcia_mem_unmap(pf, window)					\
	(pcmcia_chip_mem_unmap((pf)->sc->pct, (pf)->sc->pch, (window)))

void	*pcmcia_intr_establish(struct pcmcia_function *, int,
	    int (*) (void *), void *);
void 	pcmcia_intr_disestablish(struct pcmcia_function *, void *);

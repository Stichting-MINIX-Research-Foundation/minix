/*	$NetBSD: ahcisatavar.h,v 1.17 2015/05/24 22:30:05 jmcneill Exp $	*/

/*
 * Copyright (c) 2006 Manuel Bouyer.
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
 *
 */

#include <dev/ic/ahcisatareg.h>

#define AHCI_DEBUG

#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#define DEBUG_DETACH 0x20
#ifdef AHCI_DEBUG
extern int ahcidebug_mask;
#define AHCIDEBUG_PRINT(args, level) \
        if (ahcidebug_mask & (level)) \
		printf args
#else
#define AHCIDEBUG_PRINT(args, level)
#endif

struct ahci_softc {
	struct atac_softc sc_atac;
	bus_space_tag_t sc_ahcit; /* ahci registers mapping */
	bus_space_handle_t sc_ahcih;
	bus_size_t sc_ahcis;
	bus_dma_tag_t sc_dmat; /* DMA memory mappings: */
	void *sc_cmd_hdr; /* command tables and received FIS */
	bus_dmamap_t sc_cmd_hdrd;
	bus_dma_segment_t sc_cmd_hdr_seg;
	int sc_cmd_hdr_nseg;
	int sc_atac_capflags;
	int sc_ahci_quirks;
#define AHCI_PCI_QUIRK_FORCE	__BIT(0)  /* force attach */
#define AHCI_PCI_QUIRK_BAD64	__BIT(1)  /* broken 64-bit DMA */
#define AHCI_QUIRK_BADPMP	__BIT(2)  /* broken PMP support, ignore */
#define AHCI_QUIRK_BADPMPRESET	__BIT(3)  /* broken PMP support for reset */
#define AHCI_QUIRK_SKIP_RESET	__BIT(4)  /* skip drive reset sequence */

	uint32_t sc_ahci_cap;	/* copy of AHCI_CAP */
	int sc_ncmds; /* number of command slots */
	uint32_t sc_ahci_ports;
	struct ata_channel *sc_chanarray[AHCI_MAX_PORTS];
	struct ahci_channel {
		struct ata_channel ata_channel; /* generic part */
		bus_space_handle_t ahcic_scontrol;
		bus_space_handle_t ahcic_sstatus;
		bus_space_handle_t ahcic_serror;
		/* pointers allocated from sc_cmd_hdrd */
		struct ahci_r_fis *ahcic_rfis; /* received FIS */
		bus_addr_t ahcic_bus_rfis;
		struct ahci_cmd_header *ahcic_cmdh; /* command headers */
		bus_addr_t ahcic_bus_cmdh;
		/* command tables (allocated per-channel) */
		bus_dmamap_t ahcic_cmd_tbld;
		bus_dma_segment_t ahcic_cmd_tbl_seg;
		int ahcic_cmd_tbl_nseg;
		struct ahci_cmd_tbl *ahcic_cmd_tbl[AHCI_MAX_CMDS];
		bus_addr_t ahcic_bus_cmd_tbl[AHCI_MAX_CMDS];
		bus_dmamap_t ahcic_datad[AHCI_MAX_CMDS];
		uint32_t  ahcic_cmds_active; /* active commands */
	} sc_channels[AHCI_MAX_PORTS];

	void	(*sc_channel_start)(struct ahci_softc *, struct ata_channel *);
	void	(*sc_channel_stop)(struct ahci_softc *, struct ata_channel *);

	bool sc_save_init_data;
	struct {
		uint32_t cap;
		uint32_t cap2;
		uint32_t ports;
	} sc_init_data;
};

#define AHCINAME(sc) (device_xname((sc)->sc_atac.atac_dev))

#define AHCI_CMDH_SYNC(sc, achp, cmd, op) bus_dmamap_sync((sc)->sc_dmat, \
    (sc)->sc_cmd_hdrd, \
    (char *)(&(achp)->ahcic_cmdh[(cmd)]) - (char *)(sc)->sc_cmd_hdr, \
    sizeof(struct ahci_cmd_header), (op))
#define AHCI_RFIS_SYNC(sc, achp, op) bus_dmamap_sync((sc)->sc_dmat, \
    (sc)->sc_cmd_hdrd, (void *)(achp)->ahcic_rfis - (sc)->sc_cmd_hdr, \
    AHCI_RFIS_SIZE, (op))
#define AHCI_CMDTBL_SYNC(sc, achp, cmd, op) bus_dmamap_sync((sc)->sc_dmat, \
    (achp)->ahcic_cmd_tbld, AHCI_CMDTBL_SIZE * (cmd), \
    AHCI_CMDTBL_SIZE, (op))

#define AHCI_READ(sc, reg) bus_space_read_4((sc)->sc_ahcit, \
    (sc)->sc_ahcih, (reg))
#define AHCI_WRITE(sc, reg, val) bus_space_write_4((sc)->sc_ahcit, \
    (sc)->sc_ahcih, (reg), (val))
    

void ahci_attach(struct ahci_softc *);
int  ahci_detach(struct ahci_softc *, int);
void ahci_resume(struct ahci_softc *);

int  ahci_intr(void *);


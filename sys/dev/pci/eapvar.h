/* $NetBSD: eapvar.h,v 1.4 2011/11/23 23:07:35 jmcneill Exp $ */

#include <dev/pci/pcivar.h>

#include <dev/ic/ac97var.h>

struct eap_gameport_args {
	bus_space_tag_t gpa_iot;
	bus_space_handle_t gpa_ioh;
};

struct eap_dma {
	bus_dmamap_t map;
	void *addr;
	bus_dma_segment_t segs[1];
	int nsegs;
	size_t size;
	struct eap_dma *next;
};

#define DMAADDR(p) ((p)->map->dm_segs[0].ds_addr)
#define KERNADDR(p) ((void *)((p)->addr))

/*
 * The card has two DACs. Using them is a bit twisted: we use DAC2
 * as default and DAC1 as the optional secondary DAC.
 */
#define EAP_DAC1 1
#define EAP_DAC2 0
#define EAP_I1 EAP_DAC2
#define EAP_I2 EAP_DAC1
struct eap_instance {
	device_t parent;
	int index;

	void	(*ei_pintr)(void *);	/* DMA completion intr handler */
	void	*ei_parg;		/* arg for ei_intr() */
	device_t ei_audiodev;		/* audio device, for detach */
#ifdef DIAGNOSTIC
	char	ei_prun;
#endif
};

struct eap_softc {
	device_t sc_dev;		/* base device */
	void *sc_ih;			/* interrupt vectoring */
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t iosz;
	bus_dma_tag_t sc_dmatag;	/* DMA tag */
	kmutex_t sc_intr_lock;
	kmutex_t sc_lock;

	struct eap_dma *sc_dmas;

	void	(*sc_rintr)(void *);	/* DMA completion intr handler */
	void	*sc_rarg;		/* arg for sc_intr() */
#ifdef DIAGNOSTIC
	char	sc_rrun;
#endif

#if NMIDI > 0
	void	(*sc_iintr)(void *, int); /* midi input ready handler */
	void	(*sc_ointr)(void *);	/* midi output ready handler */
	void	*sc_arg;
	device_t sc_mididev;
#endif
#if NJOY_EAP > 0
	device_t sc_gameport;
#endif

	u_short	sc_port[AK_NPORTS];	/* mirror of the hardware setting */
	u_int	sc_record_source;	/* recording source mask */
	u_int	sc_input_source;	/* input source mask */
	u_int	sc_mic_preamp;
	char    sc_1371;		/* Using ES1371/AC97 codec */

	struct ac97_codec_if *codec_if;
	struct ac97_host_if host_if;

	struct eap_instance sc_ei[2];

	pci_chipset_tag_t sc_pc;	/* For detach */
};


device_t eap_joy_attach(device_t, struct eap_gameport_args *);
int eap_joy_detach(device_t, struct eap_gameport_args *);

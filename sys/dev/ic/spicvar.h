/* $NetBSD: spicvar.h,v 1.7 2010/02/24 22:37:58 dyoung Exp $ */

#include <dev/sysmon/sysmonvar.h>

struct spic_softc {
	device_t sc_dev;

	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;

	struct callout sc_poll;

	int sc_buttons;
	char sc_enabled;

	device_t sc_wsmousedev;

#define	SPIC_PSWITCH_LID	0
#define	SPIC_PSWITCH_SUSPEND	1
#define	SPIC_PSWITCH_HIBERNATE	2
#define	SPIC_NPSWITCH		3	
	struct sysmon_pswitch sc_smpsw[SPIC_NPSWITCH];
};

void spic_attach(struct spic_softc *);
bool spic_suspend(device_t, const pmf_qual_t *);
bool spic_resume(device_t, const pmf_qual_t *);

int spic_intr(void *);

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_types.h>

#include <net/if_ether.h>
#include <net/if_media.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

int
ether_mediachange(struct ifnet *ifp)
{
	struct ethercom *ec = (struct ethercom *)ifp;
	int rc;

	KASSERT(ec->ec_mii != NULL);

	if ((ifp->if_flags & IFF_UP) == 0)
		return 0;
	if ((rc = mii_mediachg(ec->ec_mii)) == ENXIO)
		return 0;
	return rc;
}

void
ether_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct ethercom	*ec = (struct ethercom	*)ifp;
	struct mii_data		*mii;

	KASSERT(ec->ec_mii != NULL);

#ifdef notyet
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}
#endif

	mii = ec->ec_mii;

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

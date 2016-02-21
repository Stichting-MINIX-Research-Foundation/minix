/*
 * This file contains the table with socket driver mappings.  One socket driver
 * may implement multiple domains (e.g., PF_INET and PF_INET6).  For this
 * reason, we assign a unique number to each socket driver, and use a "socket
 * device map" table (smap) that maps from those numbers to information about
 * socket drivers.  This number is combined with a per-driver socket identifier
 * to form a globally unique socket ID (64-bit, stored as dev_t).  In addition,
 * we use a table that maps from PF_xxx domains to socket drivers (pfmap).
 */

#include "fs.h"
#include <sys/socket.h>
#include <assert.h>

static struct smap smap[NR_SOCKDEVS];
static struct smap *pfmap[PF_MAX];

/*
 * Initialize the socket device map table.
 */
void
init_smap(void)
{
	unsigned int i;

	for (i = 0; i < __arraycount(smap); i++) {
		/*
		 * The smap numbers are one-based so as to ensure that no
		 * socket will have the device number NO_DEV, which would
		 * create problems with eg the select code.
		 */
		smap[i].smap_num = i + 1;
		smap[i].smap_endpt = NONE;
	}

	memset(pfmap, 0, sizeof(pfmap));
}

/*
 * Register a socket driver.  This action can only be requested by RS.  The
 * process identified by the given DS label 'label' and endpoint 'endpt' is to
 * be responsible for sockets created in the domains as given in the 'domains'
 * array, which contains 'ndomains' elements.  Return OK upon successful
 * registration, or an error code otherwise.
 */
int
smap_map(const char * label, endpoint_t endpt, const int * domains,
	unsigned int ndomains)
{
	struct smap *sp;
	unsigned int i, num = 0;
	int domain;

	if (ndomains <= 0 || ndomains > NR_DOMAIN)
		return EINVAL;

	/*
	 * See if there is already a socket device map entry for this label.
	 * If so, the socket driver is probably being restarted, and we should
	 * overwrite its previous entry.
	 */
	sp = NULL;
	for (i = 0; i < __arraycount(smap); i++) {
		if (smap[i].smap_endpt != NONE &&
		    !strcmp(smap[i].smap_label, label)) {
			sp = &smap[i];
			break;
		}
	}

	/*
	 * See if all given domains are valid and not already reserved by a
	 * socket driver other than (if applicable) this driver's old instance.
	 */
	for (i = 0; i < ndomains; i++) {
		domain = domains[i];
		if (domain < 0 || domain >= __arraycount(pfmap))
			return EINVAL;
		if (domain == PF_UNSPEC)
			return EINVAL;
		if (pfmap[domain] != NULL && pfmap[domain] != sp)
			return EBUSY;
	}

	/*
	 * If we are not about to replace an existing socket device map entry,
	 * find a free entry, returning an error if all entries are in use.
	 */
	if (sp == NULL) {
		for (num = 0; num < __arraycount(smap); num++)
			if (smap[num].smap_endpt == NONE)
				break;

		if (num == __arraycount(smap))
			return ENOMEM;
	} else
		num = (unsigned int)(sp - smap);

	/*
	 * At this point, the registration will succeed, and we can start
	 * modifying tables.  Just to be sure, unmap the domain mappings for
	 * the old instance, in case it is somehow registered with a different
	 * set of domains.  Also, if the endpoint of the service has changed,
	 * cancel any operations involving the previous endpoint and invalidate
	 * any preexisting sockets.  However, for stateful restarts where the
	 * service endpoint does not change, leave things as is.
	 */
	if (sp != NULL) {
		if (sp->smap_endpt != endpt) {
			/*
			 * For stateless restarts, it is common that the new
			 * endpoint is made ready before the old endpoint is
			 * exited, so we cannot wait for the exit handling code
			 * to do these steps, as they rely on the old socket
			 * mapping still being around.
			 */
			unsuspend_by_endpt(sp->smap_endpt);

			invalidate_filp_by_sock_drv(sp->smap_num);
		}

		for (i = 0; i < __arraycount(pfmap); i++)
			if (pfmap[i] == sp)
				pfmap[i] = NULL;
	}

	/*
	 * Initialize the socket driver map entry, and set up the domain map
	 * entries.
	 */
	sp = &smap[num];
	sp->smap_endpt = endpt;
	strlcpy(sp->smap_label, label, sizeof(sp->smap_label));
	sp->smap_sel_busy = FALSE;
	sp->smap_sel_filp = NULL;

	for (i = 0; i < ndomains; i++)
		pfmap[domains[i]] = sp;

	return OK;
}

/*
 * The process with the given endpoint has exited.  If the endpoint identifies
 * a socket driver, deregister the driver and invalidate any sockets it owned.
 */
void
smap_unmap_by_endpt(endpoint_t endpt)
{
	struct smap *sp;
	unsigned int i;

	if ((sp = get_smap_by_endpt(endpt)) == NULL)
		return;

	/*
	 * Invalidation requires that the smap entry still be around, so do
	 * this before clearing the endpoint.
	 */
	invalidate_filp_by_sock_drv(sp->smap_num);

	sp->smap_endpt = NONE;

	for (i = 0; i < __arraycount(pfmap); i++)
		if (pfmap[i] == sp)
			pfmap[i] = NULL;
}

/*
 * The given endpoint has announced itself as a socket driver.
 */
void
smap_endpt_up(endpoint_t endpt)
{
	struct smap *sp;

	if ((sp = get_smap_by_endpt(endpt)) == NULL)
		return;

	/*
	 * The announcement indicates that the socket driver has either started
	 * anew or restarted statelessly.  In the second case, none of its
	 * previously existing sockets will have survived, so mark them as
	 * invalid.
	 */
	invalidate_filp_by_sock_drv(sp->smap_num);
}

/*
 * Construct a device number that combines the entry number of the given socket
 * map and the given per-driver socket identifier, thus constructing a unique
 * identifier for the socket.  Generally speaking, we use the dev_t type
 * because the value is stored as special device number (sdev) on a socket node
 * on PFS.  We use our own bit division rather than the standard major/minor
 * division because this simplifies using each half as a 32-bit value.  The
 * block/character device numbers and socket device numbers are in different
 * namespaces, and numbers may overlap (even though this is currently
 * practically impossible), so one must always test the file type first.
 */
dev_t
make_smap_dev(struct smap * sp, sockid_t sockid)
{

	assert(sp->smap_endpt != NONE);
	assert(sockid >= 0);

	return (dev_t)(((uint64_t)sp->smap_num << 32) | (uint32_t)sockid);
}

/*
 * Return a pointer to the smap structure for the socket driver associated with
 * the socket device number.  In addition, if the given socket ID pointer is
 * not NULL, store the per-driver socket identifier in it.  Return NULL if the
 * given socket device number is not a socket for a valid socket driver.
 */
struct smap *
get_smap_by_dev(dev_t dev, sockid_t * sockidp)
{
	struct smap *sp;
	unsigned int num;
	sockid_t id;

	num = (unsigned int)(dev >> 32);
	id = (sockid_t)(dev & ((1ULL << 32) - 1));
	if (num == 0 || num > __arraycount(smap) || id < 0)
		return NULL;

	sp = &smap[num - 1];
	assert(sp->smap_num == num);

	if (sp->smap_endpt == NONE)
		return NULL;

	if (sockidp != NULL)
		*sockidp = id;
	return sp;
}

/*
 * Return a pointer to the smap structure for the socket driver with the given
 * endpoint.  Return NULL if the endpoint does not identify a socket driver.
 */
struct smap *
get_smap_by_endpt(endpoint_t endpt)
{
	unsigned int i;

	/*
	 * TODO: this function is used rather frequently, so it would be nice
	 * to get rid of the O(n) loop here.  The get_dmap_by_endpt() function
	 * suffers from the same problem.  It might be worth adding an extra
	 * field to the fproc structure for this.
	 */
	for (i = 0; i < __arraycount(smap); i++)
		if (smap[i].smap_endpt == endpt)
			return &smap[i];

	return NULL;
}

/*
 * Return a pointer to the smap structure for the socket driver handling the
 * given domain (protocol family).  Return NULL if there is no match.
 */
struct smap *
get_smap_by_domain(int domain)
{

	if (domain < 0 || domain >= __arraycount(pfmap))
		return NULL;

	return pfmap[domain]; /* may be NULL */
}

/* LWIP service - mibtree.c - sysctl support for */
/*
 * This file acts as a dispatcher for the net.inet, net.inet6, and minix.lwip
 * sysctl trees.  It does not cover the other net.* trees; these are taken care
 * of in other source files.
 */

#include "lwip.h"

#include <minix/sysctl.h>

#define MAX_PROTO	6	/* maximum # of INET protocols with subtrees */

static struct rmib_indir net_inet_indir[MAX_PROTO];
static unsigned int net_inet_indir_count = 0;
static struct rmib_node net_inet_node =
    RMIB_SNODE(RMIB_RO, net_inet_indir, "inet", "PF_INET related settings");

#ifdef INET6
static struct rmib_indir net_inet6_indir[MAX_PROTO];
static unsigned int net_inet6_indir_count = 0;
static struct rmib_node net_inet6_node =
    RMIB_SNODE(RMIB_RO, net_inet6_indir, "inet6", "PF_INET6 related settings");
#endif /* INET6 */

#define MAX_LWIP	4	/* maximum # of miscellaneous LWIP subtrees */

static struct rmib_indir minix_lwip_indir[MAX_LWIP];
static unsigned int minix_lwip_indir_count = 0;
static struct rmib_node minix_lwip_node =
    RMIB_SNODE(RMIB_RO, minix_lwip_indir, "lwip",
	"LWIP service information and settings");

/*
 * Initialize the status module by registering the net.inet, net.inet6, and
 * minix.lwip trees with the MIB service.  Other modules must have added all
 * subtrees to those trees through mibtree_register_*() before this point.
 */
void
mibtree_init(void)
{
	const int inet_mib[] = { CTL_NET, PF_INET };
#ifdef INET6
	const int inet6_mib[] = { CTL_NET, PF_INET6 };
#endif /* INET6 */
	const int lwip_mib[] = { CTL_MINIX, MINIX_LWIP };
	int r;

	/*
	 * Register the "net.inet", "net.inet6", and "minix.lwip" subtrees with
	 * the MIB service.
	 *
	 * These calls only return local failures.  Remote failures (in the MIB
	 * service) are silently ignored.  So, we can safely panic on failure.
	 */
	if ((r = rmib_register(inet_mib, __arraycount(inet_mib),
	    &net_inet_node)) != OK)
		panic("unable to register net.inet RMIB tree: %d", r);

#ifdef INET6
	if ((r = rmib_register(inet6_mib, __arraycount(inet6_mib),
	    &net_inet6_node)) != OK)
		panic("unable to register net.inet6 RMIB tree: %d", r);
#endif /* INET6 */

	if ((r = rmib_register(lwip_mib, __arraycount(lwip_mib),
	    &minix_lwip_node)) != OK)
		panic("unable to register minix.lwip RMIB tree: %d", r);
}

/*
 * Add a subtree to the local net.inet or net.inet6 tree.  This function must
 * only be called *before* mibtree_init(), as the latter will register the
 * final tree with the MIB service.
 */
void
mibtree_register_inet(int domain, int protocol, struct rmib_node * node)
{
	struct rmib_node *parent;
	struct rmib_indir *indir;
	unsigned int i, *count;

	switch (domain) {
	case PF_INET:
		parent = &net_inet_node;
		indir = net_inet_indir;
		count = &net_inet_indir_count;
		break;
	case PF_INET6:
#ifdef INET6
		parent = &net_inet6_node;
		indir = net_inet6_indir;
		count = &net_inet6_indir_count;
		break;
#else /* !INET6 */
		return;
#endif /* !INET6 */
	default:
		panic("invalid domain %d", domain);
	}

	assert(*count < MAX_PROTO);

	/* Insertion sort. */
	for (i = 0; i < *count; i++) {
		assert(indir[i].rindir_id != (unsigned int)protocol);

		if (indir[i].rindir_id > (unsigned int)protocol)
			break;
	}

	if (i < *count)
		memmove(&indir[i + 1], &indir[i],
		    sizeof(indir[0]) * (*count - i));

	indir[i].rindir_id = protocol;
	indir[i].rindir_node = node;
	parent->rnode_size = ++*count;
}

/*
 * Add a miscellaneous subtree to the local minix.lwip tree.  This function
 * must only be called *before* mibtree_init(), as the latter will register the
 * final tree with the MIB service.  Note that the given subtrees are numbered
 * arbitrarily.  We use sparse trees here only to avoid having to declare
 * external variables, which is a bit of a hack, but with the expected low
 * number of miscellaneous subtrees there will be no performance penalty.
 */
void
mibtree_register_lwip(struct rmib_node * node)
{
	unsigned int i;

	i = minix_lwip_indir_count;

	assert(i < __arraycount(minix_lwip_indir));

	minix_lwip_indir[i].rindir_id = i;
	minix_lwip_indir[i].rindir_node = node;
	minix_lwip_node.rnode_size = ++minix_lwip_indir_count;
}

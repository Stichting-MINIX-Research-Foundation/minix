/* MIB service - kern.c - implementation of the CTL_KERN subtree */

#include "mib.h"

static struct mib_node mib_kern_table[] = {
/* 8*/	[KERN_ARGMAX]		= MIB_INT(_P | _RO, ARG_MAX, "argmax",
				    "Maximum number of bytes of arguments to "
				    "execve(2)"),
};

/*
 * Initialize the CTL_KERN subtree.
 */
void
mib_kern_init(struct mib_node * node)
{

	MIB_INIT_ENODE(node, mib_kern_table);
}

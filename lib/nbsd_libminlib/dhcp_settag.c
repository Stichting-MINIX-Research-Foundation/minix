/*	dhcp_init(), dhcp_settag()			Author: Kees J. Bot
 *								1 Dec 2000
 */
#define nil ((void*)0)
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/dhcp.h>

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

void dhcp_init(dhcp_t *dp)
{
    /* Initialize a DHCP packet. */
    memset(dp, 0, offsetof(dhcp_t, magic));
    dp->magic= DHCP_MAGIC;
    memset(dp->options, 255, sizeof(dp->options));
}

int dhcp_settag(dhcp_t *dp, int tag, void *data, size_t len)
{
    /* Add a tag to a DHCP packet.  No padding.  Only do the options field.
     * (This is Minix, we don't need megabytes of silly bits of data.)
     * The length may be zero to delete a tag.
     */
    u8_t *p;
    int n;

    if (tag <= 0 || tag >= 255) return 0;

    for (p= dp->options; p < arraylimit(dp->options) && *p != 255; p += n) {
	n= 1 + 1 + p[1];
	if (*p == tag) {
	    /* The tag is already there, remove it so it gets replaced. */
	    memmove(p, p + n, arraylimit(dp->options) - (p + n));
	    memset(arraylimit(dp->options) - n, 255, n);
	    n= 0;
	}
    }

    /* Add tag. */
    if (len == 0) {
	/* We're merely deleting a tag. */
    } else
    if (p + 1 + 1 + len <= arraylimit(dp->options)) {
	*p++ = tag;
	*p++ = len;
	memcpy(p, data, len);
    } else {
	/* Oops, it didn't fit?  Is this really Minix??? */
	return 0;
    }
    return 1;
}

/* $Header$ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include <sys/types.h>
#include <unistd.h>
#include <arch.h>
#include "object.h"

#include "arch.h"
#include "archiver.h"

int
rd_arhdr(int fd, register struct ar_hdr	*arhdr)
{
	char buf[AR_TOTAL];
	register char *c = buf;
	register char *p = arhdr->ar_name;
	register int i;

	i = read(fd, c, AR_TOTAL);
	if (i == 0) return 0;
	if (i != AR_TOTAL) {
		rd_fatal();
	}
	i = 14;
	while (i--) {
		*p++ = *c++;
	}
	arhdr->ar_date = ((long) get2(c)) << 16; c += 2;
	arhdr->ar_date |= ((long) get2(c)) & 0xffff; c += 2;
	arhdr->ar_uid = *c++;
	arhdr->ar_gid = *c++;
	arhdr->ar_mode = get2(c); c += 2;
	arhdr->ar_size = (long) get2(c) << 16; c += 2;
	arhdr->ar_size |= (long) get2(c) & 0xffff;
	return 1;
}

/* $Header$ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include <ranlib.h>
#include "object.h"
#include "wr_bytes.h"
#include "ranlib.h"

void wr_ranlib(int fd, struct ranlib *ran, long cnt)
{
#if ! (BYTES_REVERSED || WORDS_REVERSED)
	if (sizeof (struct ranlib) != SZ_RAN)
#endif
	{
		char buf[100 * SZ_RAN];

		while (cnt) {
			register int i = (cnt > 100) ? 100 : cnt;
			register char *c = buf;
			long j = i * SZ_RAN;

			cnt -= i;
			while (i--) {
				put4(ran->ran_off,c); c += 4;
				put4(ran->ran_pos,c); c += 4;
				ran++;
			}
			wr_bytes(fd, buf, j);
		}
	}
#if ! (BYTES_REVERSED || WORDS_REVERSED)
	else	wr_bytes(fd, (char *) ran, cnt * SZ_RAN);
#endif
}

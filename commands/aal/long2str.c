/* $Header$ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* Integer to String translator
	-> base is a value from [-16,-2] V [2,16]
	-> base < 0: see 'val' as unsigned value
	-> no checks for buffer overflow and illegal parameters
	(1985, EHB)
*/

#define MAXWIDTH 32

char *
long2str(val, base)
	register long val;
	register int base;
{
	static char numbuf[MAXWIDTH];
	static char vec[] = "0123456789ABCDEF";
	register char *p = &numbuf[MAXWIDTH];
	int sign = (base > 0);

	*--p = '\0';		/* null-terminate string	*/
	if (val) {
		if (base > 0) {
			if (val < 0L) {
				long v1 = -val;
				if (v1 == val)
					goto overflow;
				val = v1;
			}
			else
				sign = 0;
		}
		else
		if (base < 0) {			/* unsigned */
			base = -base;
			if (val < 0L) {	/* taken from Amoeba src */
				register int mod, i;
			overflow:
				mod = 0;
				for (i = 0; i < 8 * sizeof val; i++) {
					mod <<= 1;
					if (val < 0)
						mod++;
					val <<= 1;
					if (mod >= base) {
						mod -= base;
						val++;
					}
				}
				*--p = vec[mod];
			}
		}
		do {
			*--p = vec[(int) (val % base)];
			val /= base;
		} while (val != 0L);
		if (sign)
			*--p = '-';	/* don't forget it !!	*/
	}
	else
		*--p = '0';		/* just a simple 0	*/
	return p;
}

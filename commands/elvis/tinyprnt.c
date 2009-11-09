/* tinyprnt.c */

#if OSK
#define sprintf Sprintf
#endif

/* This is a limited version of sprintf().  It is useful for Minix-PC and
 * Coherent-286 because those systems are both limited to 64k+64k and the
 * standard sprintf() is just too damn big.
 *
 * It should also be useful for OS-9 because OS-9's sprintf() doesn't
 * understand the true meaning of asterisks in a format string.  This one
 * does.
 */

/* Place-holders in format strings look like "%<pad><clip><type>".
 *
 * The <pad> adds space to the front (or, if negative, to the back) of the
 * output value, to pad it to a given width.  If <pad> is absent, then 0 is
 * assumed.  If <pad> is an asterisk, then the next argument is assumed to
 * be an (int) which used as the pad width.
 *
 * The <clip> string can be absent, in which case no clipping is done.
 * However, if it is present, then it should be either a "." followed by
 * a number, or a "." followed by an asterisk.  The asterisk means that the
 * next argument is an (int) which should be used as the pad width.  Clipping
 * only affects strings; for other data types it is ignored.
 *
 * The <type> is one of "s" for strings, "c" for characters (really ints that
 * are assumed to be legal char values), "d" for ints, "ld" for long ints, or
 * "%" to output a percent sign.
 */

/* NOTE: Variable argument lists are handled by direct stack-twiddling. Sorry! */

static void cvtnum(buf, num, base)
	char		*buf;	/* where to store the number */
	unsigned long	num;	/* the number to convert */
	int		base;	/* either 8, 10, or 16 */
{
	static char	digits[] = "0123456789abcdef";
	unsigned long	tmp;

	/* if the number is 0, then just stuff a "0" into the buffer */
	if (num == 0L)
	{
		buf[0] = '0';
		buf[1] = '\0';
		return;
	}

	/* use tmp to figure out how many digits we'll need */
	for (tmp = num; tmp > 0; tmp /= base)
	{
		buf++;
	}

	/* mark the spot that will be the end of the string */
	*buf = '\0';

	/* generate all digits, as needed */
	for (tmp = num; tmp > 0; tmp /= base)
	{
		*--buf = digits[tmp % base];
	}
}

int sprintf(buf, fmt, argref)
	char	*buf;	/* where to deposit the formatted output */
	char	*fmt;	/* the format string */
	int	argref;	/* the first argument is located at &argref */
{
	char	*argptr;/* pointer to next argument on the stack */
	int	pad;	/* value of the pad string */
	int	clip;	/* value of the clip string */
	long	num;	/* a binary number being converted to ASCII digits */
	long	digit;	/* used during conversion */
	char	*src, *dst;

	/* make argptr point to the first argument after the format string */
	argptr = (char *)&argref;

	/* loop through the whole format string */
	while (*fmt)
	{
		/* if not part of a place-holder, then copy it literally */
		if (*fmt != '%')
		{
			*buf++ = *fmt++;
			continue;
		}

		/* found a place-holder!  Get <pad> value */
		fmt++;
		if ('*' == *fmt)
		{
			pad = *((int *)argptr)++;
			fmt++;
		}
		else if (*fmt == '-' || (*fmt >= '0' && *fmt <= '9'))
		{
			pad = atol(fmt);
			do
			{
				fmt++;
			} while (*fmt >= '0' && *fmt <= '9');
		}
		else
		{
			pad = 0;
		}

		/* get a <clip> value */
		if (*fmt == '.')
		{
			fmt++;
			if ('*' == *fmt)
			{
				clip = *((int *)argptr)++;
				fmt++;
			}
			else if (*fmt >= '0' && *fmt <= '9')
			{
				clip = atol(fmt);
				do
				{
					fmt++;
				} while (*fmt >= '0' && *fmt <= '9');
			}
		}
		else
		{
			clip = 0;
		}

		/* handle <type>, possibly noticing <clip> */
		switch (*fmt++)
		{
		  case 'c':
			buf[0] = *((int *)argptr)++;
			buf[1] = '\0';
			break;

		  case 's':
			src = *((char **)argptr)++;
			if (!src)
			{
				src = "(null)";
			}
			if (clip)
			{
				strncpy(buf, src, clip);
				buf[clip] = '\0';
			}
			else
			{
				strcpy(buf, src);
			}
			break;

		  case 'l':
			fmt++; /* to skip the "d" in "%ld" */
			num = *((long *)argptr)++;
			dst = buf;
			if (num < 0)
			{
				*dst++ = '-';
				num = -num;
			}
			cvtnum(dst, num, 10);
			break;

		  case 'x':
			num = *((int *)argptr)++;
			cvtnum(buf, num, 16);
			break;

		  case 'd':
			num = *((int *)argptr)++;
			dst = buf;
			if (num < 0)
			{
				*dst++ = '-';
				num = -num;
			}
			cvtnum(dst, num, 10);
			break;

		  default:
			buf[0] = fmt[-1];
			buf[1] = '\0';
		}

		/* now fix the padding, if the value is too short */
		clip = strlen(buf);
		if (pad < 0)
		{
			/* add spaces after the value */
			pad = -pad - clip;
			for (buf += clip; pad > 0; pad--)
			{
				*buf++ = ' ';
			}
			*buf = '\0';
		}
		else
		{
			/* add spaces before the value */
			pad -= clip;
			if (pad > 0)
			{
				src = buf + clip;
				dst = src + pad;
				*dst = '\0';
				while (src > buf)
				{
					*--dst = *--src;
				}
				while (dst > buf)
				{
					*--dst = ' ';
				}
			}
			buf += strlen(buf);
		}
	}

	/* mark the end of the output string */
	*buf = '\0';
}

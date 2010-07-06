/*
 * doprnt.c - print formatted output
 */
/* $Header$ */

#include	<ctype.h>
#include	<stdio.h>
#include	<stdarg.h>
#include	<string.h>
#include	"loc_incl.h"

/* gnum() is used to get the width and precision fields of a format. */
static const char *
gnum(register const char *f, int *ip, va_list *app)
{
	register int	i, c;

	if (*f == '*') {
		*ip = va_arg((*app), int);
		f++;
	} else {
		i = 0;
		while ((c = *f - '0') >= 0 && c <= 9) {
			i = i*10 + c;
			f++;
		}
		*ip = i;
	}
	return f;
}

#if	_EM_WSIZE == _EM_PSIZE
#define set_pointer(flags)				/* nothing */
#elif	_EM_LSIZE == _EM_PSIZE
#define set_pointer(flags)	(flags |= FL_LONG)
#else
#error garbage pointer size
#define set_pointer(flags)		/* compilation might continue */
#endif

/* print an ordinal number */
static char *
o_print(va_list *ap, int flags, char *s, char c, int precision, int is_signed)
{
	printval_s_t signed_val;
	printval_u_t unsigned_val;
	char *old_s = s;
	int base;

	switch (flags & (FL_SHORT | FL_LONG | FL_LONGLONG)) {
	case FL_SHORT:
		if (is_signed) {
			signed_val = (short) va_arg(*ap, int);
		} else {
			unsigned_val = (unsigned short) va_arg(*ap, unsigned);
		}
		break;
	case FL_LONG:
		if (is_signed) {
			signed_val = va_arg(*ap, long);
		} else {
			unsigned_val = va_arg(*ap, unsigned long);
		}
		break;
#if defined(__LONG_LONG_SUPPORTED)
	case FL_LONG | FL_LONGLONG:
		if (is_signed) {
			signed_val = va_arg(*ap, long long);
		} else {
			unsigned_val = va_arg(*ap, unsigned long long);
		}
		break;
#endif
	default:
		if (is_signed) {
			signed_val = va_arg(*ap, int);
		} else {
			unsigned_val = va_arg(*ap, unsigned int);
		}
		break;
	}

	if (is_signed) {
		if (signed_val < 0) {
			*s++ = '-';
			signed_val = -signed_val;
		} else if (flags & FL_SIGN) *s++ = '+';
		else if (flags & FL_SPACE) *s++ = ' ';
		unsigned_val = signed_val;
	}
	if ((flags & FL_ALT) && (c == 'o')) *s++ = '0';
	if (!unsigned_val && c != 'p') {
		 if (!precision)
			return s;
	} else if (((flags & FL_ALT) && (c == 'x' || c == 'X'))
		    || c == 'p') {
		*s++ = '0';
		*s++ = (c == 'X' ? 'X' : 'x');
	}

	switch (c) {
	case 'b':	base = 2;	break;
	case 'o':	base = 8;	break;
	case 'd':
	case 'i':
	case 'u':	base = 10;	break;
	case 'x':
	case 'X':
	case 'p':	base = 16;	break;
	default:	base = 10;	break;
	}

	s = _i_compute(unsigned_val, base, s, precision);

	if (c == 'X')
		while (old_s != s) {
			*old_s = toupper(*old_s);
			old_s++;
		}

	return s;
}

int
_doprnt(register const char *fmt, va_list ap, FILE *stream)
{
	register char	*s;
	register int	j;
	int		i, c, width, precision, zfill, flags, between_fill;
	int		nrchars=0;
	const char	*oldfmt;
	char		*s1, buf[1025];

	while ((c = *fmt++)) {
		if (c != '%') {
#ifdef	CPM
			if (c == '\n') {
				if (putc('\r', stream) == EOF)
					return nrchars ? -nrchars : -1;
				nrchars++;
			}
#endif
			if (putc(c, stream) == EOF)
				return nrchars ? -nrchars : -1;
			nrchars++;
			continue;
		}
		flags = 0;
		do {
			switch(*fmt) {
			case '-':	flags |= FL_LJUST;	break;
			case '+':	flags |= FL_SIGN;	break;
			case ' ':	flags |= FL_SPACE;	break;
			case '#':	flags |= FL_ALT;	break;
			case '0':	flags |= FL_ZEROFILL;	break;
			default:	flags |= FL_NOMORE;	continue;
			}
			fmt++;
		} while(!(flags & FL_NOMORE));

		oldfmt = fmt;
		fmt = gnum(fmt, &width, &ap);
		if (fmt != oldfmt) flags |= FL_WIDTHSPEC;

		if (*fmt == '.') {
			fmt++; oldfmt = fmt;
			fmt = gnum(fmt, &precision, &ap);
			if (precision >= 0) flags |= FL_PRECSPEC;
		}

		if ((flags & FL_WIDTHSPEC) && width < 0) {
			width = -width;
			flags |= FL_LJUST;
		}
		if (!(flags & FL_WIDTHSPEC)) width = 0;

		if (flags & FL_SIGN) flags &= ~FL_SPACE;

		if (flags & FL_LJUST) flags &= ~FL_ZEROFILL;


		s = s1 = buf;

		flags &= ~FL_NOMORE;
		do {
			switch (*fmt) {
			case 'h':
				flags |= FL_SHORT;
				break;
			case 'j':	
				flags |= FL_LONG;
#if defined(__LONG_LONG_SUPPORTED)
				flags |= FL_LONGLONG;
#endif
				break;
			case 'l':	
				if(flags & FL_LONG)
					flags |= FL_LONGLONG;
				else
					flags |= FL_LONG;
				break;
			case 'L':
				flags |= FL_LONGDOUBLE;
				break;
			default:
				flags |= FL_NOMORE;
				continue;
			}
			fmt++;
		} while(!(flags & FL_NOMORE));

		switch (c = *fmt++) {
		default:
#ifdef	CPM
			if (c == '\n') {
				if (putc('\r', stream) == EOF)
					return nrchars ? -nrchars : -1;
				nrchars++;
			}
#endif
			if (putc(c, stream) == EOF)
				return nrchars ? -nrchars : -1;
			nrchars++;
			continue;
		case 'n':
			if (flags & FL_SHORT)
				*va_arg(ap, short *) = (short) nrchars;
			else if (flags & FL_LONG)
				*va_arg(ap, long *) = (long) nrchars;
			else
				*va_arg(ap, int *) = (int) nrchars;
			continue;
		case 's':
			s1 = va_arg(ap, char *);
			if (s1 == NULL)
				s1 = "(null)";
			s = s1;
			while (precision || !(flags & FL_PRECSPEC)) {
				if (*s == '\0')
					break;
				s++;
				precision--;
			}
			break;
		case 'p':
			set_pointer(flags);
			/* fallthrough */
		case 'b':
		case 'o':
		case 'u':
		case 'x':
		case 'X':
			if (!(flags & FL_PRECSPEC)) precision = 1;
			else if (c != 'p') flags &= ~FL_ZEROFILL;
			s = o_print(&ap, flags, s, c, precision, 0);
			break;
		case 'd':
		case 'i':
			flags |= FL_SIGNEDCONV;
			if (!(flags & FL_PRECSPEC)) precision = 1;
			else flags &= ~FL_ZEROFILL;
			s = o_print(&ap, flags, s, c, precision, 1);
			break;
		case 'c':
			*s++ = va_arg(ap, int);
			break;
#ifndef NOFLOAT
		case 'G':
		case 'g':
			if ((flags & FL_PRECSPEC) && (precision == 0))
				precision = 1;
		case 'f':
		case 'E':
		case 'e':
			if (!(flags & FL_PRECSPEC)) 
				precision = 6;

			if (precision >= sizeof(buf))
				precision = sizeof(buf) - 1;

			flags |= FL_SIGNEDCONV;
			s = _f_print(&ap, flags, s, c, precision);
			break;
#endif	/* NOFLOAT */
		case 'r':
			ap = va_arg(ap, va_list);
			fmt = va_arg(ap, char *);
			continue;
		}
		zfill = ' ';
		if (flags & FL_ZEROFILL) zfill = '0';
		j = s - s1;

		/* between_fill is true under the following conditions:
		 * 1- the fill character is '0'
		 * and
		 * 2a- the number is of the form 0x... or 0X...
		 * or
		 * 2b- the number contains a sign or space
		 */
		between_fill = 0;
		if ((flags & FL_ZEROFILL)
		    && (((c == 'x' || c == 'X') && (flags & FL_ALT) && j > 1)
			|| (c == 'p')
			|| ((flags & FL_SIGNEDCONV)
			    && ( *s1 == '+' || *s1 == '-' || *s1 == ' '))))
			between_fill++;

		if ((i = width - j) > 0)
			if (!(flags & FL_LJUST)) {	/* right justify */
				nrchars += i;
				if (between_fill) {
				    if (flags & FL_SIGNEDCONV) {
					j--; nrchars++;
					if (putc(*s1++, stream) == EOF)
						return nrchars ? -nrchars : -1;
				    } else {
					j -= 2; nrchars += 2;
					if ((putc(*s1++, stream) == EOF)
					    || (putc(*s1++, stream) == EOF))
						return nrchars ? -nrchars : -1;
				    }
				}
				do {
					if (putc(zfill, stream) == EOF)
						return nrchars ? -nrchars : -1;
				} while (--i);
			}

		nrchars += j;
		while (--j >= 0) {
			if (putc(*s1++, stream) == EOF)
				return nrchars ? -nrchars : -1;
		}

		if (i > 0) nrchars += i;
		while (--i >= 0)
			if (putc(zfill, stream) == EOF)
				return nrchars ? -nrchars : -1;
	}
	return nrchars;
}

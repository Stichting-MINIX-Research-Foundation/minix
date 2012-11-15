
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

/* vprintf() uses kputc() to print characters. */
void kputc(int c);

#define count_kputc(c) do { charcount++; putf((c), farg); } while(0)

int __fvprintf(void (*putf)(int, void *), const char *fmt, va_list argp, void *farg)
{
	int c, charcount = 0;
	enum { LEFT, RIGHT } adjust;
	enum { LLONG, LONG, INT } intsize;
	int fill;
	int width, max, len, base;
	static char X2C_tab[]= "0123456789ABCDEF";
	static char x2c_tab[]= "0123456789abcdef";
	char *x2c;
	char *p;
	long long i;
	unsigned long long u = 0;
	char temp[8 * sizeof(long long) / 3 + 2];

	while ((c= *fmt++) != 0) {
		if (c != '%') {
			/* Ordinary character. */
			count_kputc(c);
			continue;
		}

		/* Format specifier of the form:
		 *	%[adjust][fill][width][.max]keys
		 */
		c= *fmt++;

		adjust= RIGHT;
		if (c == '-') {
			adjust= LEFT;
			c= *fmt++;
		}

		fill= ' ';
		if (c == '0') {
			fill= '0';
			c= *fmt++;
		}

		width= 0;
		if (c == '*') {
			/* Width is specified as an argument, e.g. %*d. */
			width= va_arg(argp, int);
			c= *fmt++;
		} else
		if (isdigit(c)) {
			/* A number tells the width, e.g. %10d. */
			do {
				width= width * 10 + (c - '0');
			} while (isdigit(c= *fmt++));
		}

		max= INT_MAX;
		if (c == '.') {
			/* Max field length coming up. */
			if ((c= *fmt++) == '*') {
				max= va_arg(argp, int);
				c= *fmt++;
			} else
			if (isdigit(c)) {
				max= 0;
				do {
					max= max * 10 + (c - '0');
				} while (isdigit(c= *fmt++));
			}
		}

		/* Set a few flags to the default. */
		x2c= x2c_tab;
		i= 0;
		base= 10;
		intsize= INT;
		if (c == 'l' || c == 'L') {
			/* "Long" key, e.g. %ld. */
			intsize= LONG;
			c= *fmt++;
		}
		if (c == 'l' || c == 'L') {
			/* "Long long" key, e.g. %lld. */
			intsize= LLONG;
			c= *fmt++;
		}
		if (c == 0) break;

		switch (c) {
			/* Decimal. */
		case 'd':
			switch (intsize) {
			case LLONG: i= va_arg(argp, long long); break;
			case LONG: i= va_arg(argp, long); break;
			case INT: i= va_arg(argp, int); break;
			}
			u= i < 0 ? -i : i;
			goto int2ascii;

			/* Octal. */
		case 'o':
			base= 010;
			goto getint;

			/* Pointer, interpret as %X or %lX. */
		case 'p':
			if (sizeof(char *) > sizeof(long)) intsize= LLONG;
			else if (sizeof(char *) > sizeof(int)) intsize= LONG;

			/* Hexadecimal.  %X prints upper case A-F, not %lx. */
		case 'X':
			x2c= X2C_tab;
		case 'x':
			base= 0x10;
			goto getint;

			/* Unsigned decimal. */
		case 'u':
		getint:
			switch (intsize) {
			case LLONG: u= va_arg(argp, unsigned long long); break;
			case LONG: u= va_arg(argp, unsigned long); break;
			case INT: u= va_arg(argp, unsigned int); break;
			}
		int2ascii:
			p= temp + sizeof(temp)-1;
			*p= 0;
			do {
				*--p= x2c[(ptrdiff_t) (u % base)];
			} while ((u /= base) > 0);
			goto string_length;

			/* A character. */
		case 'c':
			p= temp;
			*p= va_arg(argp, int);
			len= 1;
			goto string_print;

			/* Simply a percent. */
		case '%':
			p= temp;
			*p= '%';
			len= 1;
			goto string_print;

			/* A string.  The other cases will join in here. */
		case 's':
			p= va_arg(argp, char *);
			if (!p) p = "(null)";

		string_length:
			for (len= 0; p[len] != 0 && len < max; len++) {}

		string_print:
			width -= len;
			if (i < 0) width--;
			if (fill == '0' && i < 0) count_kputc('-');
			if (adjust == RIGHT) {
				while (width > 0) { count_kputc(fill); width--; }
			}
			if (fill == ' ' && i < 0) count_kputc('-');
			while (len > 0) { count_kputc((unsigned char) *p++); len--; }
			while (width > 0) { count_kputc(fill); width--; }
			break;

			/* Unrecognized format key, echo it back. */
		default:
			count_kputc('%');
			count_kputc(c);
		}
	}

	/* Mark the end with a null (should be something else, like -1). */
	kputc(0);

	return charcount;
}

#include <sys/cdefs.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

__weak_alias(vprintf, _vprintf)
__weak_alias(vfprintf, _vfprintf)
__strong_alias(__vfprintf_unlocked, _vfprintf)

static void
__xfputc(int c, void *arg)
{
	FILE *fp = (FILE *)arg;
	if (fp->_flags & __SSTR) {
		/* Write to a string. */
		if (fp->_w == 0)
			return;
		memset(fp->_p++, c, 1);
		fp->_w -= 1;
		return;
	}

	/* Not a string. Print it. */
	kputc(c);
}

int _vprintf(const char *fmt, va_list argp)
{
	return __fvprintf(__xfputc, fmt, argp, stdout);
}

int _vfprintf(FILE *fp, const char *fmt, va_list argp)
{
	return	__fvprintf(__xfputc, fmt, argp, fp);
}

/*
 * $PchId: kprintf.c,v 1.5 1996/04/11 06:59:05 philip Exp $
 */

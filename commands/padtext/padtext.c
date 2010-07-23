/* padtext: pad out the text segment of a separate I&D a.out binary to a 
 * multiple of CLICK_SIZE, used for mult-boot
 *
 * author: Erik van der Kouwe, vdkouwe@cs.vu.nl, June 9th 2010
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <minix/a.out.h>
#include <minix/config.h>
#include <minix/const.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* padding: x86 opcode int 3 (breakpoint) to ensure a trap if the padding area 
 * accidentally gets executed
 */
#define PADDING_CHAR ((unsigned char) 0xcc)

static const char *argv0, *pathin, *pathout;
static int fdin, fdout;

static int readfixed(void *buffer, size_t count)
{
	ssize_t r;

	while (count > 0) {
		/* read as many bytes as possible */
		r = read(fdin, buffer, count);
		if (r <= 0) {
			if (r < 0) {
				fprintf(stderr, 
					"%s: %s: error while reading: %s\n", 
					argv0, pathin, strerror(errno));
			} else {
				fprintf(stderr, 
					"%s: %s: premature end of file, "
					"expected %u more bytes\n", 
					argv0, pathin, count);
			}
			return -1;
		}

		/* maybe we need to read another block */
		buffer = (char *) buffer + r;
		count -= r;
	}

	return 0;
}

static int writefixed(const void *buffer, size_t count)
{
	ssize_t r;

	while (count > 0) {
		/* read as many bytes as possible */
		r = write(fdout, buffer, count);
		if (r <= 0) {
			if (r < 0) {
				fprintf(stderr, 
					"%s: %s: error while writing: %s\n", 
					argv0, pathout, strerror(errno));
			} else {
				fprintf(stderr, 
					"%s: %s: premature end of file\n", 
					argv0, pathout);
			}
			return -1;
		}

		/* maybe we need to read another block */
		buffer = (const char *) buffer + r;
		count -= r;
	}

	return 0;
}

static int writepadding(size_t padsize)
{
	char buffer[CLICK_SIZE];

	/* we never write more than a single click */
	assert(padsize <= sizeof(buffer));
	memset(buffer, PADDING_CHAR, padsize);
	return writefixed(buffer, padsize);
}

static int copyfixed(size_t count)
{
	char buffer[4096];
	size_t countnow;

	while (count > 0) {
		/* copying a fixed number of bytes, we expect everything to 
		 * succeed 
		 */ 
		countnow = (count < sizeof(buffer)) ? count : sizeof(buffer);
		if (readfixed(buffer, countnow) < 0 ||
			writefixed(buffer, countnow) < 0) {
			return -1;
		}
		count -= countnow;
	}

	return 0;
}

static int copyall(void)
{
	char buffer[4096];
	ssize_t r;

	for (; ; ) {
		/* copy everything until EOF */ 
		r = read(fdin, buffer, sizeof(buffer));
		if (r <= 0) {
			if (r < 0) {
				fprintf(stderr,
					"%s: %s: error while reading: %s\n", 
					argv0, pathin, strerror(errno));
				return -1;
			} else {
				/* EOF, stop copying */
				break;
			}
		}

		if (writefixed(buffer, r) < 0) {
			return -1;
		}
	}

	return 0;
}

static int padtext(void)
{
	struct exec headerin, headerout;
	long padsize;

	/* read header */
	assert(A_MINHDR <= sizeof(headerin));
	if (readfixed(&headerin, A_MINHDR) < 0) {
		return -1;
	}

	/* check header sanity */
	if (BADMAG(headerin) ||
		headerin.a_hdrlen < A_MINHDR ||
		(headerin.a_flags & ~(A_NSYM | A_EXEC | A_SEP)) != 0 ||
		headerin.a_text < 0 ||
		headerin.a_data < 0 ||
		headerin.a_bss < 0 ||
		headerin.a_entry != 0 ||
		headerin.a_total < 0 ||
		headerin.a_syms < 0) {
		fprintf(stderr, "%s: %s: invalid a.out header\n", 
			argv0, pathin);
		return -1;
	}

	if (headerin.a_cpu != A_I80386) {
		fprintf(stderr, "%s: %s: not an i386 executable\n", 
			argv0, pathin);
		return -1;
	}

	if ((headerin.a_flags & A_SEP) != A_SEP) {
		fprintf(stderr, 
			"%s: %s: combined I&D, padding text is impossible\n", 
			argv0, pathin);
		return -1;
	}

	/* adjust header */
	headerout = headerin;
	padsize = CLICK_SIZE - headerout.a_text % CLICK_SIZE;
	if (padsize == CLICK_SIZE) padsize = 0;
	printf("%s: %s: adding %ld bytes of padding\n", pathin, argv0, padsize);
	headerout.a_text += padsize;

	/* write header and copy text segment */
	if (writefixed(&headerout, A_MINHDR) < 0 ||
		copyfixed(headerin.a_hdrlen - A_MINHDR + headerin.a_text) < 0 ||
		writepadding(padsize) < 0 ||
		copyall() < 0) {
		return -1;		
	}

	return 0;
}

int main(int argc, char **argv)
{
	/* check parameters */
	argv0 = argv[0];
	if (argc > 3) {
		printf("usage:\n");
		printf("  %s [infile [outfile]]\n", argv0);
		return 1;
	}

	if (argc >= 2) {
		/* open specified input file */
		pathin = argv[1];
		fdin = open(pathin, O_RDONLY);
		if (fdin < 0) {
			fprintf(stderr, 
				"%s: cannot open input file \"%s\" "
				"for reading: %s\n",
				argv0, pathin, strerror(errno));
			return -1;
		}
	} else {
		/* input from stdin */
		pathin = "-";
		fdin = STDIN_FILENO;
	}
		
	if (argc >= 3) {
		/* open specified output file */
		pathout = argv[2];
		fdout = creat(pathout, 0644);
		if (fdout < 0) {
			fprintf(stderr, 
				"%s: cannot open output file \"%s\" "
				"for writing: %s\n",
				argv0, pathout, strerror(errno));
			return -1;
		}
	} else {
		/* output to stdout */
		pathout = "-";
		fdout = STDOUT_FILENO;
	}

	/* now do the job using the file handles */
	if (padtext() < 0) {
		return -1;
	}

	return 0;
}

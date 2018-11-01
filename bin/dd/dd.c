/*	$NetBSD: dd.c,v 1.51 2016/09/05 01:00:07 sevan Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1991, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)dd.c	8.5 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: dd.c,v 1.51 2016/09/05 01:00:07 sevan Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dd.h"
#include "extern.h"

static void dd_close(void);
static void dd_in(void);
static void getfdtype(IO *);
static void redup_clean_fd(IO *);
static void setup(void);

IO		in, out;		/* input/output state */
STAT		st;			/* statistics */
void		(*cfunc)(void);		/* conversion function */
uint64_t	cpy_cnt;		/* # of blocks to copy */
static off_t	pending = 0;		/* pending seek if sparse */
u_int		ddflags;		/* conversion options */
#ifdef NO_IOFLAG
#define		iflag	O_RDONLY
#define		oflag	O_CREAT
#else
u_int		iflag = O_RDONLY;	/* open(2) flags for input file */
u_int		oflag = O_CREAT;	/* open(2) flags for output file */
#endif /* NO_IOFLAG */
uint64_t	cbsz;			/* conversion block size */
u_int		files_cnt = 1;		/* # of files to copy */
uint64_t	progress = 0;		/* display sign of life */
const u_char	*ctab;			/* conversion table */
sigset_t	infoset;		/* a set blocking SIGINFO */
const char	*msgfmt = "posix";	/* default summary() message format */

/*
 * Ops for stdin/stdout and crunch'd dd.  These are always host ops.
 */
static const struct ddfops ddfops_stdfd = {
	.op_open = open,
	.op_close = close,
	.op_fcntl = fcntl,
	.op_ioctl = ioctl,
	.op_fstat = fstat,
	.op_fsync = fsync,
	.op_ftruncate = ftruncate,
	.op_lseek = lseek,
	.op_read = read,
	.op_write = write,
};
extern const struct ddfops ddfops_prog;

int
main(int argc, char *argv[])
{
	int ch;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			errx(EXIT_FAILURE, "usage: dd [operand ...]");
			/* NOTREACHED */
		}
	}
	argc -= (optind - 1);
	argv += (optind - 1);

	jcl(argv);
#ifndef CRUNCHOPS
	if (ddfops_prog.op_init && ddfops_prog.op_init() == -1)
		err(1, "prog init");
#endif
	setup();

	(void)signal(SIGINFO, summaryx);
	(void)signal(SIGINT, terminate);
	(void)sigemptyset(&infoset);
	(void)sigaddset(&infoset, SIGINFO);

	(void)atexit(summary);

	while (files_cnt--)
		dd_in();

	dd_close();
	exit(0);
	/* NOTREACHED */
}

static void
setup(void)
{
#ifdef CRUNCHOPS
	const struct ddfops *prog_ops = &ddfops_stdfd;
#else
	const struct ddfops *prog_ops = &ddfops_prog;
#endif

	if (in.name == NULL) {
		in.name = "stdin";
		in.fd = STDIN_FILENO;
		in.ops = &ddfops_stdfd;
	} else {
		in.ops = prog_ops;
		in.fd = ddop_open(in, in.name, iflag, 0);
		if (in.fd < 0)
			err(EXIT_FAILURE, "%s", in.name);
			/* NOTREACHED */

		/* Ensure in.fd is outside the stdio descriptor range */
		redup_clean_fd(&in);
	}

	getfdtype(&in);

	if (files_cnt > 1 && !(in.flags & ISTAPE)) {
		errx(EXIT_FAILURE, "files is not supported for non-tape devices");
		/* NOTREACHED */
	}

	if (out.name == NULL) {
		/* No way to check for read access here. */
		out.fd = STDOUT_FILENO;
		out.name = "stdout";
		out.ops = &ddfops_stdfd;
	} else {
		out.ops = prog_ops;

#ifndef NO_IOFLAG
		if ((oflag & O_TRUNC) && (ddflags & C_SEEK)) {
			errx(EXIT_FAILURE, "oflag=trunc is incompatible "
			     "with seek or oseek operands, giving up.");
			/* NOTREACHED */
		}
		if ((oflag & O_TRUNC) && (ddflags & C_NOTRUNC)) {
			errx(EXIT_FAILURE, "oflag=trunc is incompatible "
			     "with conv=notrunc operand, giving up.");
			/* NOTREACHED */
		}
#endif /* NO_IOFLAG */
#define	OFLAGS \
    (oflag | (ddflags & (C_SEEK | C_NOTRUNC) ? 0 : O_TRUNC))
		out.fd = ddop_open(out, out.name, O_RDWR | OFLAGS, DEFFILEMODE);
		/*
		 * May not have read access, so try again with write only.
		 * Without read we may have a problem if output also does
		 * not support seeks.
		 */
		if (out.fd < 0) {
			out.fd = ddop_open(out, out.name, O_WRONLY | OFLAGS,
			    DEFFILEMODE);
			out.flags |= NOREAD;
		}
		if (out.fd < 0) {
			err(EXIT_FAILURE, "%s", out.name);
			/* NOTREACHED */
		}

		/* Ensure out.fd is outside the stdio descriptor range */
		redup_clean_fd(&out);
	}

	getfdtype(&out);

	/*
	 * Allocate space for the input and output buffers.  If not doing
	 * record oriented I/O, only need a single buffer.
	 */
	if (!(ddflags & (C_BLOCK|C_UNBLOCK))) {
		size_t dbsz = out.dbsz;
		if (!(ddflags & C_BS))
			dbsz += in.dbsz - 1;
		if ((in.db = malloc(dbsz)) == NULL) {
			err(EXIT_FAILURE, NULL);
			/* NOTREACHED */
		}
		out.db = in.db;
	} else if ((in.db =
	    malloc((u_int)(MAX(in.dbsz, cbsz) + cbsz))) == NULL ||
	    (out.db = malloc((u_int)(out.dbsz + cbsz))) == NULL) {
		err(EXIT_FAILURE, NULL);
		/* NOTREACHED */
	}
	in.dbp = in.db;
	out.dbp = out.db;

	/* Position the input/output streams. */
	if (in.offset)
		pos_in();
	if (out.offset)
		pos_out();

	/*
	 * Truncate the output file; ignore errors because it fails on some
	 * kinds of output files, tapes, for example.
	 */
	if ((ddflags & (C_OF | C_SEEK | C_NOTRUNC)) == (C_OF | C_SEEK))
		(void)ddop_ftruncate(out, out.fd, (off_t)out.offset * out.dbsz);

	/*
	 * If converting case at the same time as another conversion, build a
	 * table that does both at once.  If just converting case, use the
	 * built-in tables.
	 */
	if (ddflags & (C_LCASE|C_UCASE)) {
#ifdef	NO_CONV
		/* Should not get here, but just in case... */
		errx(EXIT_FAILURE, "case conv and -DNO_CONV");
		/* NOTREACHED */
#else	/* NO_CONV */
		u_int cnt;

		if (ddflags & C_ASCII || ddflags & C_EBCDIC) {
			if (ddflags & C_LCASE) {
				for (cnt = 0; cnt < 256; ++cnt)
					casetab[cnt] = tolower(ctab[cnt]);
			} else {
				for (cnt = 0; cnt < 256; ++cnt)
					casetab[cnt] = toupper(ctab[cnt]);
			}
		} else {
			if (ddflags & C_LCASE) {
				for (cnt = 0; cnt < 256; ++cnt)
					casetab[cnt] = tolower(cnt);
			} else {
				for (cnt = 0; cnt < 256; ++cnt)
					casetab[cnt] = toupper(cnt);
			}
		}

		ctab = casetab;
#endif	/* NO_CONV */
	}

	(void)gettimeofday(&st.start, NULL);	/* Statistics timestamp. */
}

static void
getfdtype(IO *io)
{
	struct mtget mt;
	struct stat sb;

	if (io->ops->op_fstat(io->fd, &sb)) {
		err(EXIT_FAILURE, "%s", io->name);
		/* NOTREACHED */
	}
	if (S_ISCHR(sb.st_mode))
		io->flags |= io->ops->op_ioctl(io->fd, MTIOCGET, &mt)
		    ? ISCHR : ISTAPE;
	else if (io->ops->op_lseek(io->fd, (off_t)0, SEEK_CUR) == -1
	    && errno == ESPIPE)
		io->flags |= ISPIPE;		/* XXX fixed in 4.4BSD */
}

/*
 * Move the parameter file descriptor to a descriptor that is outside the
 * stdio descriptor range, if necessary.  This is required to avoid
 * accidentally outputting completion or error messages into the
 * output file that were intended for the tty.
 */
static void
redup_clean_fd(IO *io)
{
	int fd = io->fd;
	int newfd;

	if (fd != STDIN_FILENO && fd != STDOUT_FILENO &&
	    fd != STDERR_FILENO)
		/* File descriptor is ok, return immediately. */
		return;

	/*
	 * 3 is the first descriptor greater than STD*_FILENO.  Any
	 * free descriptor valued 3 or above is acceptable...
	 */
	newfd = io->ops->op_fcntl(fd, F_DUPFD, 3);
	if (newfd < 0) {
		err(EXIT_FAILURE, "dupfd IO");
		/* NOTREACHED */
	}

	io->ops->op_close(fd);
	io->fd = newfd;
}

static void
dd_in(void)
{
	int flags;
	int64_t n;

	for (flags = ddflags;;) {
		if (cpy_cnt && (st.in_full + st.in_part) >= cpy_cnt)
			return;

		/*
		 * Clear the buffer first if doing "sync" on input.
		 * If doing block operations use spaces.  This will
		 * affect not only the C_NOERROR case, but also the
		 * last partial input block which should be padded
		 * with zero and not garbage.
		 */
		if (flags & C_SYNC) {
			if (flags & (C_BLOCK|C_UNBLOCK))
				(void)memset(in.dbp, ' ', in.dbsz);
			else
				(void)memset(in.dbp, 0, in.dbsz);
		}

		n = ddop_read(in, in.fd, in.dbp, in.dbsz);
		if (n == 0) {
			in.dbrcnt = 0;
			return;
		}

		/* Read error. */
		if (n < 0) {

			/*
			 * If noerror not specified, die.  POSIX requires that
			 * the warning message be followed by an I/O display.
			 */
			if (!(flags & C_NOERROR)) {
				err(EXIT_FAILURE, "%s", in.name);
				/* NOTREACHED */
			}
			warn("%s", in.name);
			summary();

			/*
			 * If it's not a tape drive or a pipe, seek past the
			 * error.  If your OS doesn't do the right thing for
			 * raw disks this section should be modified to re-read
			 * in sector size chunks.
			 */
			if (!(in.flags & (ISPIPE|ISTAPE)) &&
			    ddop_lseek(in, in.fd, (off_t)in.dbsz, SEEK_CUR))
				warn("%s", in.name);

			/* If sync not specified, omit block and continue. */
			if (!(ddflags & C_SYNC))
				continue;

			/* Read errors count as full blocks. */
			in.dbcnt += in.dbrcnt = in.dbsz;
			++st.in_full;

		/* Handle full input blocks. */
		} else if ((uint64_t)n == in.dbsz) {
			in.dbcnt += in.dbrcnt = n;
			++st.in_full;

		/* Handle partial input blocks. */
		} else {
			/* If sync, use the entire block. */
			if (ddflags & C_SYNC)
				in.dbcnt += in.dbrcnt = in.dbsz;
			else
				in.dbcnt += in.dbrcnt = n;
			++st.in_part;
		}

		/*
		 * POSIX states that if bs is set and no other conversions
		 * than noerror, notrunc or sync are specified, the block
		 * is output without buffering as it is read.
		 */
		if (ddflags & C_BS) {
			out.dbcnt = in.dbcnt;
			dd_out(1);
			in.dbcnt = 0;
			continue;
		}

		if (ddflags & C_SWAB) {
			if ((n = in.dbrcnt) & 1) {
				++st.swab;
				--n;
			}
			swab(in.dbp, in.dbp, n);
		}

		in.dbp += in.dbrcnt;
		(*cfunc)();
	}
}

/*
 * Cleanup any remaining I/O and flush output.  If necessary, output file
 * is truncated.
 */
static void
dd_close(void)
{

	if (cfunc == def)
		def_close();
	else if (cfunc == block)
		block_close();
	else if (cfunc == unblock)
		unblock_close();
	if (ddflags & C_OSYNC && out.dbcnt < out.dbsz) {
		(void)memset(out.dbp, 0, out.dbsz - out.dbcnt);
		out.dbcnt = out.dbsz;
	}
	/* If there are pending sparse blocks, make sure
	 * to write out the final block un-sparse
	 */
	if ((out.dbcnt == 0) && pending) {
		memset(out.db, 0, out.dbsz);
		out.dbcnt = out.dbsz;
		out.dbp = out.db + out.dbcnt;
		pending -= out.dbsz;
	}
	if (out.dbcnt)
		dd_out(1);

	/*
	 * Reporting nfs write error may be deferred until next
	 * write(2) or close(2) system call.  So, we need to do an
	 * extra check.  If an output is stdout, the file structure
	 * may be shared with other processes and close(2) just
	 * decreases the reference count.
	 */
	if (out.fd == STDOUT_FILENO && ddop_fsync(out, out.fd) == -1
	    && errno != EINVAL) {
		err(EXIT_FAILURE, "fsync stdout");
		/* NOTREACHED */
	}
	if (ddop_close(out, out.fd) == -1) {
		err(EXIT_FAILURE, "close");
		/* NOTREACHED */
	}
}

void
dd_out(int force)
{
	static int warned;
	int64_t cnt, n, nw;
	u_char *outp;

	/*
	 * Write one or more blocks out.  The common case is writing a full
	 * output block in a single write; increment the full block stats.
	 * Otherwise, we're into partial block writes.  If a partial write,
	 * and it's a character device, just warn.  If a tape device, quit.
	 *
	 * The partial writes represent two cases.  1: Where the input block
	 * was less than expected so the output block was less than expected.
	 * 2: Where the input block was the right size but we were forced to
	 * write the block in multiple chunks.  The original versions of dd(1)
	 * never wrote a block in more than a single write, so the latter case
	 * never happened.
	 *
	 * One special case is if we're forced to do the write -- in that case
	 * we play games with the buffer size, and it's usually a partial write.
	 */
	outp = out.db;
	for (n = force ? out.dbcnt : out.dbsz;; n = out.dbsz) {
		for (cnt = n;; cnt -= nw) {

			if (!force && ddflags & C_SPARSE) {
				int sparse, i;
				sparse = 1;	/* Is buffer sparse? */
				for (i = 0; i < cnt; i++)
					if (outp[i] != 0) {
						sparse = 0;
						break;
					}
				if (sparse) {
					pending += cnt;
					outp += cnt;
					nw = 0;
					break;
				}
			}
			if (pending != 0) {
				if (ddop_lseek(out,
				    out.fd, pending, SEEK_CUR) == -1)
					err(EXIT_FAILURE, "%s: seek error creating sparse file",
					    out.name);
			}
			nw = bwrite(&out, outp, cnt);
			if (nw <= 0) {
				if (nw == 0)
					errx(EXIT_FAILURE,
						"%s: end of device", out.name);
					/* NOTREACHED */
				if (errno != EINTR)
					err(EXIT_FAILURE, "%s", out.name);
					/* NOTREACHED */
				nw = 0;
			}
			if (pending) {
				st.bytes += pending;
				st.sparse += pending/out.dbsz;
				st.out_full += pending/out.dbsz;
				pending = 0;
			}
			outp += nw;
			st.bytes += nw;
			if (nw == n) {
				if ((uint64_t)n != out.dbsz)
					++st.out_part;
				else
					++st.out_full;
				break;
			}
			++st.out_part;
			if (nw == cnt)
				break;
			if (out.flags & ISCHR && !warned) {
				warned = 1;
				warnx("%s: short write on character device", out.name);
			}
			if (out.flags & ISTAPE)
				errx(EXIT_FAILURE,
					"%s: short write on tape device", out.name);
				/* NOTREACHED */

		}
		if ((out.dbcnt -= n) < out.dbsz)
			break;
	}

	/* Reassemble the output block. */
	if (out.dbcnt)
		(void)memmove(out.db, out.dbp - out.dbcnt, out.dbcnt);
	out.dbp = out.db + out.dbcnt;

	if (progress && (st.out_full + st.out_part) % progress == 0)
		(void)write(STDERR_FILENO, ".", 1);
}

/*
 * A protected against SIGINFO write
 */
ssize_t
bwrite(IO *io, const void *buf, size_t len)
{
	sigset_t oset;
	ssize_t rv;
	int oerrno;

	(void)sigprocmask(SIG_BLOCK, &infoset, &oset);
	rv = io->ops->op_write(io->fd, buf, len);
	oerrno = errno;
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	errno = oerrno;
	return (rv);
}

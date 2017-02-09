/*	$NetBSD: rump_allserver.c,v 1.39 2015/04/16 10:05:43 pooka Exp $	*/

/*-
 * Copyright (c) 2010, 2011 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <rump/rumpuser_port.h>

#ifndef lint
__RCSID("$NetBSD: rump_allserver.c,v 1.39 2015/04/16 10:05:43 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>
#include <rump/rumpdefs.h>
#include <rump/rumperr.h>

#if defined(__minix)

/*
 * XXX: This is the worst POSIX semaphore emulation ever created by mankind.
 */

#define sem_t volatile int
#define sem_init(a,b,c) *a = 1
#define sem_post(a) *a = 0
#define sem_wait(a) do { sleep(1); } while (*a);
#endif

__dead static void
usage(void)
{

#ifndef HAVE_GETPROGNAME
#define getprogname() "rump_server"
#endif
	fprintf(stderr, "usage: %s [-s] [-c ncpu] [-d drivespec] [-l libs] "
	    "[-m modules] bindurl\n", getprogname());
	exit(1);
}

__dead static void
diedie(int sflag, const char *reason, int error, const char *errstr)
{

	if (reason != NULL)
		fputs(reason, stderr);
	if (errstr) {
		fprintf(stderr, ": %s", errstr);
	}
	fputc('\n', stderr);
	if (!sflag)
		rump_daemonize_done(error);
	exit(1);
}

__dead static void
die(int sflag, int error, const char *reason)
{

	diedie(sflag, reason, error, error == 0 ? NULL : strerror(error));
}

__dead static void
die_rumperr(int sflag, int error, const char *reason)
{

	diedie(sflag, reason, error, error == 0 ? NULL : rump_strerror(error));
}

static sem_t sigsem;
static void
sigreboot(int sig)
{

	sem_post(&sigsem);
}

static const char *const disktokens[] = {
#define DKEY 0
	"key",
#define DFILE 1
	"hostpath",
#define DSIZE 2
#define DSIZE_E -1
	"size",
#define DOFFSET 3
	"offset",
#define DLABEL 4
	"disklabel",
#define DTYPE 5
	"type",
	NULL
};

struct etfsreg {
	const char *key;
	const char *hostpath;
	off_t flen;
	off_t foffset;
	char partition;
	enum rump_etfs_type type;
};

struct etfstype {
	const char *name;
	enum rump_etfs_type type;
} etfstypes[] = {
	{ "blk", RUMP_ETFS_BLK },
	{ "chr", RUMP_ETFS_CHR },
	{ "reg", RUMP_ETFS_REG },
};

static void processlabel(int, int, int, off_t *, off_t *);

#define ALLOCCHUNK 32

int
main(int argc, char *argv[])
{
	const char *serverurl;
	struct etfsreg *etfs = NULL;
	unsigned netfs = 0, curetfs = 0;
	int error;
	int ch, sflag, onthepath;
	unsigned i;
	char **modarray = NULL, **libarray = NULL;
	unsigned nmods = 0, curmod = 0, nlibs = 0, curlib = 0, libidx;
	unsigned liblast = -1; /* XXXgcc */

	setprogname(argv[0]);
	sflag = 0;
	while ((ch = getopt(argc, argv, "c:d:l:m:r:sv")) != -1) {
		switch (ch) {
		case 'c':
			setenv("RUMP_NCPU", optarg, 1);
			break;
		case 'd': {
			char *options, *value;
			char *key, *hostpath;
			long long flen, foffset;
			char partition;
			int ftype;

			flen = foffset = 0;
			partition = 0;
			key = hostpath = NULL;
			ftype = -1;
			options = optarg;
			while (*options) {
				switch (getsubopt(&options,
				    __UNCONST(disktokens), &value)) {
				case DKEY:
					if (key != NULL) {
						fprintf(stderr,
						    "key already given\n");
						usage();
					}
					key = value;
					break;

				case DFILE:
					if (hostpath != NULL) {
						fprintf(stderr,
						    "hostpath already given\n");
						usage();
					}
					hostpath = value;
					break;

				case DSIZE:
					if (flen != 0) {
						fprintf(stderr,
						    "size already given\n");
						usage();
					}
					if (strcmp(value, "host") == 0) {
						if (foffset != 0) {
							fprintf(stderr,
							    "cannot specify "
							    "offset with "
							    "size=host\n");
							usage();
						}
						flen = DSIZE_E;
					} else {
#ifdef HAVE_STRSUFTOLL
						/* XXX: off_t max? */
						flen = strsuftoll("-d size",
						    value, 0, LLONG_MAX);
#else
						flen = strtoull(value,
						    NULL, 10);
#endif
					}
					break;
				case DOFFSET:
					if (foffset != 0) {
						fprintf(stderr,
						    "offset already given\n");
						usage();
					}
					if (flen == DSIZE_E) {
						fprintf(stderr, "cannot "
						    "specify offset with "
						    "size=host\n");
						usage();
					}
#ifdef HAVE_STRSUFTOLL
					/* XXX: off_t max? */
					foffset = strsuftoll("-d offset", value,
					    0, LLONG_MAX);
#else
					foffset = strtoull(value, NULL, 10);
#endif
					break;

				case DLABEL:
					if (foffset != 0 || flen != 0) {
						fprintf(stderr,
						    "disklabel needs to be "
						    "used alone\n");
						usage();
					}
					if (strlen(value) != 1 ||
					    *value < 'a' || *value > 'z') {
						fprintf(stderr,
						    "invalid label part\n");
						usage();
					}
					partition = *value;
					break;

				case DTYPE:
					if (ftype != -1) {
						fprintf(stderr,
						    "type already specified\n");
						usage();
					}

					for (i = 0;
					    i < __arraycount(etfstypes);
					    i++) {
						if (strcmp(etfstypes[i].name,
						    value) == 0)
							break;
					}
					if (i == __arraycount(etfstypes)) {
						fprintf(stderr,
						    "invalid type %s\n", value);
						usage();
					}
					ftype = etfstypes[i].type;
					break;

				default:
					fprintf(stderr, "invalid dtoken\n");
					usage();
					break;
				}
			}

			if (key == NULL || hostpath == NULL ||
			    (flen == 0
			      && partition == 0 && ftype != RUMP_ETFS_REG)) {
				fprintf(stderr, "incomplete drivespec\n");
				usage();
			}
			if (ftype == -1)
				ftype = RUMP_ETFS_BLK;

			if (netfs - curetfs == 0) {
				etfs = realloc(etfs,
				    (netfs+ALLOCCHUNK)*sizeof(*etfs));
				if (etfs == NULL)
					die(1, errno, "realloc etfs");
				netfs += ALLOCCHUNK;
			}

			etfs[curetfs].key = key;
			etfs[curetfs].hostpath = hostpath;
			etfs[curetfs].flen = flen;
			etfs[curetfs].foffset = foffset;
			etfs[curetfs].partition = partition;
			etfs[curetfs].type = ftype;
			curetfs++;

			break;
		}
		case 'l':
			if (nlibs - curlib == 0) {
				libarray = realloc(libarray,
				    (nlibs+ALLOCCHUNK) * sizeof(char *));
				if (libarray == NULL)
					die(1, errno, "realloc");
				nlibs += ALLOCCHUNK;
			}
			libarray[curlib++] = optarg;
			break;
		case 'm':
			if (nmods - curmod == 0) {
				modarray = realloc(modarray,
				    (nmods+ALLOCCHUNK) * sizeof(char *));
				if (modarray == NULL)
					die(1, errno, "realloc");
				nmods += ALLOCCHUNK;
			}
			modarray[curmod++] = optarg;
			break;
		case 'r':
			setenv("RUMP_MEMLIMIT", optarg, 1);
			break;
		case 's':
			sflag = 1;
			break;
		case 'v':
			setenv("RUMP_VERBOSE", "1", 1);
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/*
	 * Automatically "resolve" component dependencies, i.e.
	 * try to load libs in a loop until all are loaded or a
	 * full loop completes with no loads (latter case is an error).
	 */
	for (onthepath = 1, nlibs = curlib; onthepath && nlibs > 0;) {
		onthepath = 0;
		for (libidx = 0; libidx < curlib; libidx++) {
			/* loaded already? */
			if (libarray[libidx] == NULL)
				continue;

			/* try to load */
			liblast = libidx;
			if (dlopen(libarray[libidx],
			    RTLD_LAZY|RTLD_GLOBAL) == NULL) {
				char pb[MAXPATHLEN];
				/* try to mimic linker -l syntax */
				snprintf(pb, sizeof(pb),
				    "lib%s.so", libarray[libidx]);
				if (dlopen(pb, RTLD_LAZY|RTLD_GLOBAL) == NULL)
					continue;
			}

			/* managed to load that one */
			libarray[libidx] = NULL;
			nlibs--;
			onthepath = 1;
		}
	}
	if (nlibs > 0) {
		fprintf(stderr,
		    "failed to load -libraries, last error from \"%s\":\n",
		    libarray[liblast]);
		fprintf(stderr, "  %s", dlerror());
		die(1, 0, NULL);
	}
	free(libarray);

	serverurl = argv[0];

	if (!sflag) {
		error = rump_daemonize_begin();
		if (error)
			die_rumperr(1, error, "rump daemonize");
	}

	error = rump_init();
	if (error)
		die_rumperr(sflag, error, "rump init failed");

	/* load modules */
	for (i = 0; i < curmod; i++) {
		struct rump_modctl_load ml;

#define ETFSKEY "/module.mod"
		if ((error = rump_pub_etfs_register(ETFSKEY,
		    modarray[0], RUMP_ETFS_REG)) != 0)
			die_rumperr(sflag,
			    error, "module etfs register failed");
		memset(&ml, 0, sizeof(ml));
		ml.ml_filename = ETFSKEY;
		/*
		 * XXX: since this is a syscall, error namespace depends
		 * on loaded emulations.  revisit and fix.
		 */
		if (rump_sys_modctl(RUMP_MODCTL_LOAD, &ml) == -1)
			die(sflag, errno, "module load failed");
		rump_pub_etfs_remove(ETFSKEY);
#undef ETFSKEY
	}
	free(modarray);

	/* register host drives */
	for (i = 0; i < curetfs; i++) {
		struct stat sb;
		off_t foffset, flen, fendoff;
		int fd, oflags;

		oflags = etfs[i].flen == DSIZE_E ? 0 : O_CREAT;
		fd = open(etfs[i].hostpath, O_RDWR | oflags, 0644);
		if (fd == -1)
			die(sflag, errno, "etfs hostpath open");

		if (etfs[i].partition) {
			processlabel(sflag, fd, etfs[i].partition - 'a',
			    &foffset, &flen);
		} else {
			foffset = etfs[i].foffset;
			flen = etfs[i].flen;
		}

		if (fstat(fd, &sb) == -1)
			die(sflag, errno, "fstat etfs hostpath");
		if (flen == DSIZE_E) {
			if (sb.st_size == 0)
				die(sflag, EINVAL, "size=host, but cannot "
				    "query non-zero size");
			flen = sb.st_size;
		}
		fendoff = foffset + flen;
		if (S_ISREG(sb.st_mode) && sb.st_size < fendoff) {
			if (ftruncate(fd, fendoff) == -1)
				die(sflag, errno, "truncate");
		}
		close(fd);

		if ((error = rump_pub_etfs_register_withsize(etfs[i].key,
		    etfs[i].hostpath, etfs[i].type, foffset, flen)) != 0)
			die_rumperr(sflag, error, "etfs register");
	}

	error = rump_init_server(serverurl);
	if (error)
		die_rumperr(sflag, error, "rump server init failed");

	if (!sflag)
		rump_daemonize_done(RUMP_DAEMONIZE_SUCCESS);

	sem_init(&sigsem, 0, 0);
	signal(SIGTERM, sigreboot);
	signal(SIGINT, sigreboot);
	sem_wait(&sigsem);

	rump_sys_reboot(0, NULL);
	/*NOTREACHED*/

	return 0;
}

/*
 * Copyright (c) 1987, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	@(#)disklabel.h	8.2 (Berkeley) 7/10/94
 */

#define	RUMPSERVER_MAXPARTITIONS	22
#define	RUMPSERVER_DISKMAGIC		((uint32_t)0x82564557)	/* magic */
#define	RUMPSERVER_DEVSHIFT		9

struct rumpserver_disklabel {
	uint32_t d_magic;		/* the magic number */
	uint16_t d_type;		/* drive type */
	uint16_t d_subtype;		/* controller/d_type specific */
	char	 d_typename[16];	/* type name, e.g. "eagle" */

	/*
	 * d_packname contains the pack identifier and is returned when
	 * the disklabel is read off the disk or in-core copy.
	 * d_boot0 and d_boot1 are the (optional) names of the
	 * primary (block 0) and secondary (block 1-15) bootstraps
	 * as found in /usr/mdec.  These are returned when using
	 * getdiskbyname(3) to retrieve the values from /etc/disktab.
	 */
	union {
		char	un_d_packname[16];	/* pack identifier */
		struct {
			char *un_d_boot0;	/* primary bootstrap name */
			char *un_d_boot1;	/* secondary bootstrap name */
		} un_b;
	} d_un;
#define	d_packname	d_un.un_d_packname
#define	d_boot0		d_un.un_b.un_d_boot0
#define	d_boot1		d_un.un_b.un_d_boot1

			/* disk geometry: */
	uint32_t d_secsize;		/* # of bytes per sector */
	uint32_t d_nsectors;		/* # of data sectors per track */
	uint32_t d_ntracks;		/* # of tracks per cylinder */
	uint32_t d_ncylinders;		/* # of data cylinders per unit */
	uint32_t d_secpercyl;		/* # of data sectors per cylinder */
	uint32_t d_secperunit;		/* # of data sectors per unit */

	/*
	 * Spares (bad sector replacements) below are not counted in
	 * d_nsectors or d_secpercyl.  Spare sectors are assumed to
	 * be physical sectors which occupy space at the end of each
	 * track and/or cylinder.
	 */
	uint16_t d_sparespertrack;	/* # of spare sectors per track */
	uint16_t d_sparespercyl;	/* # of spare sectors per cylinder */
	/*
	 * Alternative cylinders include maintenance, replacement,
	 * configuration description areas, etc.
	 */
	uint32_t d_acylinders;		/* # of alt. cylinders per unit */

			/* hardware characteristics: */
	/*
	 * d_interleave, d_trackskew and d_cylskew describe perturbations
	 * in the media format used to compensate for a slow controller.
	 * Interleave is physical sector interleave, set up by the
	 * formatter or controller when formatting.  When interleaving is
	 * in use, logically adjacent sectors are not physically
	 * contiguous, but instead are separated by some number of
	 * sectors.  It is specified as the ratio of physical sectors
	 * traversed per logical sector.  Thus an interleave of 1:1
	 * implies contiguous layout, while 2:1 implies that logical
	 * sector 0 is separated by one sector from logical sector 1.
	 * d_trackskew is the offset of sector 0 on track N relative to
	 * sector 0 on track N-1 on the same cylinder.  Finally, d_cylskew
	 * is the offset of sector 0 on cylinder N relative to sector 0
	 * on cylinder N-1.
	 */
	uint16_t d_rpm;		/* rotational speed */
	uint16_t d_interleave;		/* hardware sector interleave */
	uint16_t d_trackskew;		/* sector 0 skew, per track */
	uint16_t d_cylskew;		/* sector 0 skew, per cylinder */
	uint32_t d_headswitch;		/* head switch time, usec */
	uint32_t d_trkseek;		/* track-to-track seek, usec */
	uint32_t d_flags;		/* generic flags */
#define	NDDATA 5
	uint32_t d_drivedata[NDDATA];	/* drive-type specific information */
#define	NSPARE 5
	uint32_t d_spare[NSPARE];	/* reserved for future use */
	uint32_t d_magic2;		/* the magic number (again) */
	uint16_t d_checksum;		/* xor of data incl. partitions */

			/* filesystem and partition information: */
	uint16_t d_npartitions;	/* number of partitions in following */
	uint32_t d_bbsize;		/* size of boot area at sn0, bytes */
	uint32_t d_sbsize;		/* max size of fs superblock, bytes */
	struct	rumpserver_partition {	/* the partition table */
		uint32_t p_size;	/* number of sectors in partition */
		uint32_t p_offset;	/* starting sector */
		union {
			uint32_t fsize; /* FFS, ADOS:
					    filesystem basic fragment size */
			uint32_t cdsession; /* ISO9660: session offset */
		} __partition_u2;
#define	p_fsize		__partition_u2.fsize
#define	p_cdsession	__partition_u2.cdsession
		uint8_t p_fstype;	/* filesystem type, see below */
		uint8_t p_frag;	/* filesystem fragments per block */
		union {
			uint16_t cpg;	/* UFS: FS cylinders per group */
			uint16_t sgs;	/* LFS: FS segment shift */
		} __partition_u1;
#define	p_cpg	__partition_u1.cpg
#define	p_sgs	__partition_u1.sgs
	} d_partitions[RUMPSERVER_MAXPARTITIONS];	/* actually may be more */
};


/* for swapping disklabel, so don't care about perf, just portability */
#define bs32(x) \
	((((x) & 0xff000000) >> 24)| \
	(((x) & 0x00ff0000) >>  8) | \
	(((x) & 0x0000ff00) <<  8) | \
	(((x) & 0x000000ff) << 24))
#define bs16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))

/*
 * From:
 *	$NetBSD: disklabel_dkcksum.c,v 1.4 2005/05/15 21:01:34 thorpej Exp
 */

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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

static uint16_t
rs_dl_dkcksum(struct rumpserver_disklabel *lp, int imswapped)
{
	uint16_t *start, *end;
	uint16_t sum;
	uint16_t npart;

	if (imswapped)
		npart = bs16(lp->d_npartitions);
	else
		npart = lp->d_npartitions;

	sum = 0;
	start = (uint16_t *)(void *)lp;
	end = (uint16_t *)(void *)&lp->d_partitions[npart];
	while (start < end) {
		if (imswapped)
			sum ^= bs16(*start);
		else
			sum ^= *start;
		start++;
	}
	return (sum);
}

/*
 * From:
 * NetBSD: disklabel_scan.c,v 1.3 2009/01/18 12:13:03 lukem Exp
 */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

static int
rs_dl_scan(struct rumpserver_disklabel *lp, int *isswapped,
	char *buf, size_t buflen)
{
	size_t i;
	int imswapped;
	uint16_t npart;

	/* scan for the correct magic numbers. */

	for (i=0; i <= buflen - sizeof(*lp); i += 4) {
		memcpy(lp, buf + i, sizeof(*lp));
		if (lp->d_magic == RUMPSERVER_DISKMAGIC &&
		    lp->d_magic2 == RUMPSERVER_DISKMAGIC) {
			imswapped = 0;
			goto sanity;
		}
		if (lp->d_magic == bs32(RUMPSERVER_DISKMAGIC) &&
		    lp->d_magic2 == bs32(RUMPSERVER_DISKMAGIC)) {
			imswapped = 1;
			goto sanity;
		}
	}

	return 1;

sanity:
	if (imswapped)
		npart = bs16(lp->d_npartitions);
	else
		npart = lp->d_npartitions;
	/* we've found something, let's sanity check it */
	if (npart > RUMPSERVER_MAXPARTITIONS
	    || rs_dl_dkcksum(lp, imswapped))
		return 1;

	*isswapped = imswapped;
	return 0;
}

static void
processlabel(int sflag, int fd, int partition, off_t *foffp, off_t *flenp)
{
	struct rumpserver_disklabel dl;
	char buf[1<<16];
	uint32_t foffset, flen;
	int imswapped;

	if (pread(fd, buf, sizeof(buf), 0) == -1)
		die(sflag, errno, "could not read disk device");
	if (rs_dl_scan(&dl, &imswapped, buf, sizeof(buf)))
		die(sflag, ENOENT, "disklabel not found");

	if (partition >= dl.d_npartitions)
		die(sflag, ENOENT, "partition not available");

	foffset = dl.d_partitions[partition].p_offset << RUMPSERVER_DEVSHIFT;
	flen = dl.d_partitions[partition].p_size << RUMPSERVER_DEVSHIFT;
	if (imswapped) {
		foffset = bs32(foffset);
		flen = bs32(flen);
	}

	*foffp = (off_t)foffset;
	*flenp = (off_t)flen;
}

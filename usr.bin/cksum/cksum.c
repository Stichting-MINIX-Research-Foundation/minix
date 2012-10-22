/*	$NetBSD: cksum.c,v 1.45 2011/08/29 14:12:29 joerg Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James W. Williams of NASA Goddard Space Flight Center.
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

/*-
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James W. Williams of NASA Goddard Space Flight Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(__COPYRIGHT) && !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 1991, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#if defined(__RCSID) && !defined(lint)
#if 0
static char sccsid[] = "@(#)cksum.c	8.2 (Berkeley) 4/28/95";
#endif
__RCSID("$NetBSD: cksum.c,v 1.45 2011/08/29 14:12:29 joerg Exp $");
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <md2.h>
#include <md4.h>
#include <md5.h>
#include <rmd160.h>
#include <sha1.h>
#include <sha2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

typedef char *(*_filefunc)(const char *, char *);

const struct hash {
	const char *progname;
	const char *hashname;
	void (*stringfunc)(const char *);
	void (*timetrialfunc)(void);
	void (*testsuitefunc)(void);
	void (*filterfunc)(int);
	char *(*filefunc)(const char *, char *);
} hashes[] = {
	{ "md2", "MD2",
	  MD2String, MD2TimeTrial, MD2TestSuite,
	  MD2Filter, MD2File },
	{ "md4", "MD4",
	  MD4String, MD4TimeTrial, MD4TestSuite,
	  MD4Filter, MD4File },
	{ "md5", "MD5",
	  MD5String, MD5TimeTrial, MD5TestSuite,
	  MD5Filter, MD5File },
	{ "rmd160", "RMD160",
	  RMD160String, RMD160TimeTrial, RMD160TestSuite,
	  RMD160Filter, (_filefunc) RMD160File },
	{ "sha1", "SHA1",
	  SHA1String, SHA1TimeTrial, SHA1TestSuite,
	  SHA1Filter, (_filefunc) SHA1File },
	{ "sha256", "SHA256",
	  SHA256_String, SHA256_TimeTrial, SHA256_TestSuite,
	  SHA256_Filter, (_filefunc) SHA256_File },
	{ "sha384", "SHA384",
	  SHA384_String, SHA384_TimeTrial, SHA384_TestSuite,
	  SHA384_Filter, (_filefunc) SHA384_File },
	{ "sha512", "SHA512",
	  SHA512_String, SHA512_TimeTrial, SHA512_TestSuite,
	  SHA512_Filter, (_filefunc) SHA512_File },
	{ .progname = NULL, },
};

static int	hash_digest_file(char *, const struct hash *, int);
__dead static void	requirehash(const char *);
__dead static void	usage(void);

int
main(int argc, char **argv)
{
	int ch, fd, rval, dosum, pflag, nohashstdin;
	u_int32_t val;
	off_t len;
	char *fn;
	const char *progname;
	int (*cfncn) (int, u_int32_t *, off_t *);
	void (*pfncn) (char *, u_int32_t, off_t);
	const struct hash *hash;
	int normal, i, check_warn, do_check;

	cfncn = NULL;
	pfncn = NULL;
	dosum = pflag = nohashstdin = 0;
	normal = 0;
	check_warn = 0;
	do_check = 0;

	setlocale(LC_ALL, "");

	progname = getprogname();

	for (hash = hashes; hash->hashname != NULL; hash++)
		if (strcmp(progname, hash->progname) == 0)
			break;

	if (hash->hashname == NULL) {
		hash = NULL;

		if (!strcmp(progname, "sum")) {
			dosum = 1;
			cfncn = csum1;
			pfncn = psum1;
		} else {
			cfncn = crc;
			pfncn = pcrc;
		}
	}

	while ((ch = getopt(argc, argv, "a:cno:ps:twx")) != -1)
		switch(ch) {
		case 'a':
			if (hash) {
				warnx("illegal use of -a option\n");
				usage();
			}
			i = 0;
			while (hashes[i].hashname != NULL) {
				if (!strcasecmp(hashes[i].hashname, optarg)) {
					hash = &hashes[i];
					break;
				}
				i++;
			}
			if (hash == NULL) {
				if (!strcasecmp(optarg, "old1")) {
					cfncn = csum1;
					pfncn = psum1;
				} else if (!strcasecmp(optarg, "old2")) {
					cfncn = csum2;
					pfncn = psum2;
				} else if (!strcasecmp(optarg, "crc")) {
					cfncn = crc;
					pfncn = pcrc;
				} else {
					warnx("illegal argument to -a option");
					usage();
				}
			}
			break;
		case 'c':
			do_check = 1;
			break;
		case 'n':
			normal = 1;
			break;
		case 'o':
			if (hash) {
				warnx("%s mutually exclusive with sum",
				      hash->hashname);
				usage();
			}
			if (!strcmp(optarg, "1")) {
				cfncn = csum1;
				pfncn = psum1;
			} else if (!strcmp(optarg, "2")) {
				cfncn = csum2;
				pfncn = psum2;
			} else {
				warnx("illegal argument to -o option");
				usage();
			}
			break;
		case 'p':
			if (hash == NULL)
				requirehash("-p");
			pflag = 1;
			break;
		case 's':
			if (hash == NULL)
				requirehash("-s");
			nohashstdin = 1;
			hash->stringfunc(optarg);
			break;
		case 't':
			if (hash == NULL)
				requirehash("-t");
			nohashstdin = 1;
			hash->timetrialfunc();
			break;
		case 'w':
			check_warn = 1;
			break;
		case 'x':
			if (hash == NULL)
				requirehash("-x");
			nohashstdin = 1;
			hash->testsuitefunc();
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (do_check) {
		/*
		 * Verify checksums
		 */
		FILE *f;
		char buf[BUFSIZ];
		char *s, *p_filename, *p_cksum;
		int l_filename, l_cksum;
		char filename[BUFSIZ];
		char cksum[BUFSIZ];
		int ok,cnt,badcnt;

		rval = 0;
		cnt = badcnt = 0;

		if (argc == 0) {
			f = fdopen(STDIN_FILENO, "r");
		} else {
			f = fopen(argv[0], "r");
		}
		if (f == NULL)
			err(1, "Cannot read %s",
			    argc>0?argv[0]:"stdin");
		
		while(fgets(buf, sizeof(buf), f) != NULL) {
			s=strrchr(buf, '\n');
			if (s)
				*s = '\0';

			p_cksum = p_filename = NULL;

			p_filename = strchr(buf, '(');
			if (p_filename) {
				/*
				 * Assume 'normal' output if there's a '('
				 */
				p_filename += 1;
				normal = 0;

				p_cksum = strrchr(p_filename, ')');
				if (p_cksum == NULL) {
					if (check_warn)
						warnx("bogus format: %s. "
						      "Skipping...",
						      buf);
					rval = 1;
					continue;
				}
 				p_cksum += 4;

				l_cksum = strlen(p_cksum);
				l_filename = p_cksum - p_filename - 4;
					
				/* Sanity check, and find proper hash if
				 * it's not the same as the current program
				 */
				if (hash == NULL ||
				    strncmp(buf, hash->hashname,
					    strlen(hash->hashname)) != 0) {
					/*
					 * Search proper hash
					 */
					const struct hash *nhash;
					
					for (nhash = hashes ;
					     nhash->hashname != NULL;
					     nhash++)
						if (strncmp(buf,
							    nhash->hashname,
							    strlen(nhash->hashname)) == 0)
							break;
					
					
					if (nhash->hashname == NULL) {
						if (check_warn)
							warnx("unknown hash: %s",
							      buf);
						rval = 1;
						continue;
					} else {
						hash = nhash;
					}
				}

			} else {
				if (hash) {
					int nspaces;

					/*
					 * 'normal' output, no (ck)sum
					 */
					normal = 1;
					nspaces = 1;
					
					p_cksum = buf;
					p_filename = strchr(buf, ' ');
					if (p_filename == NULL) {
						if (check_warn)
							warnx("no filename in %s? "
							      "Skipping...", buf);
						rval = 1;
						continue;
					}
					while (isspace((int)*++p_filename))
						nspaces++;
					l_filename = strlen(p_filename);
					l_cksum = p_filename - buf - nspaces;
				} else {
					/*
					 * sum/cksum output format
					 */
					p_cksum = buf;
					s=strchr(p_cksum, ' ');
					if (s == NULL) {
						if (check_warn)
							warnx("bogus format: %s."
							      " Skipping...",
							      buf);
						rval = 1;
						continue;
					}
					l_cksum = s - p_cksum;

					p_filename = strrchr(buf, ' ');
					if (p_filename == NULL) {
						if (check_warn)
							warnx("no filename in %s?"
							      " Skipping...",
							      buf);
						rval = 1;
						continue;
					}
					p_filename++;
					l_filename = strlen(p_filename);
				}
			}

			strlcpy(filename, p_filename, l_filename+1);
			strlcpy(cksum, p_cksum, l_cksum+1);

			if (hash) {
				if (access(filename, R_OK) == 0
				    && strcmp(cksum, hash->filefunc(filename, NULL)) == 0)
					ok = 1;
				else
					ok = 0;
			} else {
				if ((fd = open(filename, O_RDONLY, 0)) < 0) {
					if (check_warn)
						warn("%s", filename);
					rval = 1;
					ok = 0;
				} else {
					if (cfncn(fd, &val, &len)) 
						ok = 0;
					else {
						u_int32_t should_val;
						
						should_val =
						  strtoul(cksum, NULL, 10);
						if (val == should_val)
							ok = 1;
						else
							ok = 0;
					}
					close(fd);
				}
			}

			if (! ok) {
				if (hash)
					printf("(%s) ", hash->hashname);
				printf("%s: FAILED\n", filename);
				badcnt++;
			}
			cnt++;

		}
		fclose(f);

		if (badcnt > 0) 
			rval = 1;
		
	} else {
		/*
		 * Calculate checksums
		 */

		fd = STDIN_FILENO;
		fn = NULL;
		rval = 0;
		do {
			if (*argv) {
				fn = *argv++;
				if (hash != NULL) {
					if (hash_digest_file(fn, hash, normal)) {
						warn("%s", fn);
						rval = 1;
					}
					continue;
				}
				if ((fd = open(fn, O_RDONLY, 0)) < 0) {
					warn("%s", fn);
					rval = 1;
					continue;
				}
			} else if (hash && !nohashstdin) {
				hash->filterfunc(pflag);
			}
			
			if (hash == NULL) {
				if (cfncn(fd, &val, &len)) {
					warn("%s", fn ? fn : "stdin");
					rval = 1;
				} else
					pfncn(fn, val, len);
				(void)close(fd);
			}
		} while (*argv);
	}
	exit(rval);
}

static int
hash_digest_file(char *fn, const struct hash *hash, int normal)
{
	char *cp;

	cp = hash->filefunc(fn, NULL);
	if (cp == NULL)
		return 1;

	if (normal)
		printf("%s %s\n", cp, fn);
	else
		printf("%s (%s) = %s\n", hash->hashname, fn, cp);

	free(cp);

	return 0;
}

static void
requirehash(const char *flg)
{
	warnx("%s flag requires `-a algorithm'", flg);
	usage();
}

static void
usage(void)
{
	const char fileargs[] = "[file ... | -c [-w] [sumfile]]";
	const char sumargs[] = "[-n] [-a algorithm [-ptx] [-s string]] [-o 1|2]";
	const char hashargs[] = "[-nptx] [-s string]";

	(void)fprintf(stderr, "usage: cksum %s\n             %s\n",
	    sumargs, fileargs);
	(void)fprintf(stderr, "       sum %s\n           %s\n",
	    sumargs, fileargs);
	(void)fprintf(stderr, "       md2 %s %s\n", hashargs, fileargs);
	(void)fprintf(stderr, "       md4 %s %s\n", hashargs, fileargs);
	(void)fprintf(stderr, "       md5 %s %s\n", hashargs, fileargs);
	(void)fprintf(stderr, "       rmd160 %s %s\n", hashargs, fileargs);
	(void)fprintf(stderr, "       sha1 %s %s\n", hashargs, fileargs);
	exit(1);
}

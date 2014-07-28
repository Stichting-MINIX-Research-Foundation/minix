/*	$OpenBSD: diffdir.c,v 1.35 2009/10/27 23:59:37 deraadt Exp $	*/

/*
 * Copyright (c) 2003 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <paths.h>
#include <sys/statfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "diff.h"
#include "xmalloc.h"

struct diffdirent {
	int d_status;
	int d_fileno;
	unsigned short d_reclen;
	char d_name[1];
};

#define roundup(x, y)      ((((x)+((y)-1))/(y))*(y))

static int dircompare(const void *, const void *);
static int excluded(const char *);
static struct diffdirent **slurpdir(char *, char **, int);
static void diffit(struct diffdirent *, char *, size_t, char *, size_t, int);

/*
 * Diff directory traversal. Will be called recursively if -r was specified.
 */
void
diffdir(char *p1, char *p2, int flags)
{
	struct diffdirent **dirp1, **dirp2, **dp1, **dp2;
	struct diffdirent *dent1, *dent2;
	size_t dirlen1, dirlen2;
	char path1[MAXPATHLEN], path2[MAXPATHLEN];
	char *dirbuf1, *dirbuf2;
	int pos;

	dirlen1 = strlcpy(path1, *p1 ? p1 : ".", sizeof(path1));
	if (dirlen1 >= sizeof(path1) - 1) {
		warnx("%s: %s", p1, strerror(ENAMETOOLONG));
		status = 2;
		return;
	}
	if (path1[dirlen1 - 1] != '/') {
		path1[dirlen1++] = '/';
		path1[dirlen1] = '\0';
	}
	dirlen2 = strlcpy(path2, *p2 ? p2 : ".", sizeof(path2));
	if (dirlen2 >= sizeof(path2) - 1) {
		warnx("%s: %s", p2, strerror(ENAMETOOLONG));
		status = 2;
		return;
	}
	if (path2[dirlen2 - 1] != '/') {
		path2[dirlen2++] = '/';
		path2[dirlen2] = '\0';
	}

	/* get a list of the entries in each directory */
	dp1 = dirp1 = slurpdir(path1, &dirbuf1, Nflag + Pflag);
	dp2 = dirp2 = slurpdir(path2, &dirbuf2, Nflag);
	if (dirp1 == NULL || dirp2 == NULL)
		return;

	/*
	 * If we were given a starting point, find it.
	 */
	if (start != NULL) {
		while (*dp1 != NULL && strcmp((*dp1)->d_name, start) < 0)
			dp1++;
		while (*dp2 != NULL && strcmp((*dp2)->d_name, start) < 0)
			dp2++;
	}

	/*
	 * Iterate through the two directory lists, diffing as we go.
	 */
	while (*dp1 != NULL || *dp2 != NULL) {
		dent1 = *dp1;
		dent2 = *dp2;

		pos = dent1 == NULL ? 1 : dent2 == NULL ? -1 :
		    strcmp(dent1->d_name, dent2->d_name);
		if (pos == 0) {
			/* file exists in both dirs, diff it */
			diffit(dent1, path1, dirlen1, path2, dirlen2, flags);
			dp1++;
			dp2++;
		} else if (pos < 0) {
			/* file only in first dir, only diff if -N */
			if (Nflag)
				diffit(dent1, path1, dirlen1, path2, dirlen2,
				    flags);
			else if (lflag)
				dent1->d_status |= D_ONLY;
			else
				print_only(path1, dirlen1, dent1->d_name);
			dp1++;
		} else {
			/* file only in second dir, only diff if -N or -P */
			if (Nflag || Pflag)
				diffit(dent2, path1, dirlen1, path2, dirlen2,
				    flags);
			else if (lflag)
				dent2->d_status |= D_ONLY;
			else
				print_only(path2, dirlen2, dent2->d_name);
			dp2++;
		}
	}
	if (lflag) {
		path1[dirlen1] = '\0';
		path2[dirlen2] = '\0';
		for (dp1 = dirp1; (dent1 = *dp1) != NULL; dp1++) {
			print_status(dent1->d_status, path1, path2,
			    dent1->d_name);
		}
		for (dp2 = dirp2; (dent2 = *dp2) != NULL; dp2++) {
			if (dent2->d_status == D_ONLY)
				print_status(dent2->d_status, path2, NULL,
				    dent2->d_name);
		}
	}

	if (dirbuf1 != NULL) {
		xfree(dirp1);
		xfree(dirbuf1);
	}
	if (dirbuf2 != NULL) {
		xfree(dirp2);
		xfree(dirbuf2);
	}
}

static int
getdiffdirentries(int fd, char *buf, int nbytes)
{
	char *read_de;
	int dentsbytes_actual;
	int i, readlen = nbytes * 11 / 16; /* worst growth */
	int written = 0;

        lseek(fd, (off_t)0, SEEK_CUR);

	if(!(read_de = malloc(readlen)))
		errx(1, "getdiffdirentries: can't malloc");

	dentsbytes_actual = getdents(fd, (struct dirent *) read_de, readlen);

	if(dentsbytes_actual <= 0)
		return dentsbytes_actual;

	while(dentsbytes_actual > 0) {
		int namelen;
		struct diffdirent *dde = (struct diffdirent *) buf;
		struct dirent *de = (struct dirent *) read_de;
		dde->d_status = 0;
		dde->d_fileno = de->d_ino;
		namelen = strlen(de->d_name);
		dde->d_reclen = namelen + 1 + sizeof(*dde) - sizeof(dde->d_name);
		strcpy(dde->d_name, de->d_name);
		dde->d_status = 0;
		assert(dentsbytes_actual >= de->d_reclen);
		dentsbytes_actual -= de->d_reclen;
		buf += dde->d_reclen;
		read_de += de->d_reclen;
		written += dde->d_reclen;
		assert(written <= nbytes);
#if 0
		fprintf(stderr, "orig: inode %d len %d; made: inode %d len %d\n",
			de->d_ino, de->d_reclen, dde->d_fileno, dde->d_reclen);
#endif
	}

	return written;
}

/*
 * Read in a whole directory's worth of struct dirents, culling
 * out the "excluded" ones.
 * Returns an array of struct diffdirent *'s that point into the buffer
 * returned via bufp.  Caller is responsible for free()ing both of these.
 */
static struct diffdirent **
slurpdir(char *path, char **bufp, int enoentok)
{
	char *buf, *ebuf, *cp;
	size_t bufsize, have, need;
	int fd, nbytes, entries;
#ifdef __minix
	struct statfs statfs;
#endif
	struct stat sb;
	int blocksize;
	struct diffdirent **dirlist, *dp;

	*bufp = NULL;
	if ((fd = open(path, O_RDONLY, 0644)) == -1) {
		static struct diffdirent *dummy;

		if (!enoentok || errno != ENOENT) {
			warn("%s", path);
			return (NULL);
		}
		return (&dummy);
	}
	if (
#ifdef __minix
		fstatfs(fd, &statfs) < 0 ||
#endif
		fstat(fd, &sb)  < 0) {
		warn("%s", path);
		close(fd);
		return (NULL);
	}
#ifdef __minix
	blocksize = statfs.f_bsize;
#else
	blocksize = sb.st_blksize;
#endif

	need = roundup(blocksize, sizeof(struct dirent));
	have = bufsize = roundup(MAX(sb.st_size, blocksize),
	    sizeof(struct dirent)) + need;
	ebuf = buf = xmalloc(bufsize);

	do {
		if (have < need) {
		    bufsize += need;
		    have += need;
		    cp = xrealloc(buf, 1, bufsize);
		    ebuf = cp + (ebuf - buf);
		    buf = cp;
		}
		nbytes = getdiffdirentries(fd, ebuf, have);
		if (nbytes == -1) {
			warn("%s", path);
			xfree(buf);
			close(fd);
			return (NULL);
		}
		ebuf += nbytes;
		have -= nbytes;
	} while (nbytes != 0);
	close(fd);

	/*
	 * We now have all the directory entries in our buffer.
	 * However, in order to easily sort them we need to convert
	 * the buffer into an array.
	 */
	for (entries = 0, cp = buf; cp < ebuf; ) {
		dp = (struct diffdirent *)cp;
		if (dp->d_fileno != 0)
			entries++;
		if (dp->d_reclen <= 0)
			break;
		cp += dp->d_reclen;
	}
	dirlist = xcalloc(sizeof(*dirlist), entries + 1);
	for (entries = 0, cp = buf; cp < ebuf; ) {
		dp = (struct diffdirent *)cp;
		if (dp->d_fileno != 0 && !excluded(dp->d_name)) {
			dp->d_status = 0;
			dirlist[entries++] = dp;
		}
		if (dp->d_reclen <= 0)
			break;
		cp += dp->d_reclen;
	}
	dirlist[entries] = NULL;

	qsort(dirlist, entries, sizeof(struct diffdirent *), dircompare);

	*bufp = buf;
	return (dirlist);
}

/*
 * Compare d_name in two dirent structures; for qsort(3).
 */
static int
dircompare(const void *vp1, const void *vp2)
{
	struct diffdirent *dp1 = *((struct diffdirent **) vp1);
	struct diffdirent *dp2 = *((struct diffdirent **) vp2);

	return (strcmp(dp1->d_name, dp2->d_name));
}

/*
 * Do the actual diff by calling either diffreg() or diffdir().
 */
static void
diffit(struct diffdirent *dp, char *path1, size_t plen1, char *path2, size_t plen2,
    int flags)
{
	flags |= D_HEADER;
	strlcpy(path1 + plen1, dp->d_name, MAXPATHLEN - plen1);
	if (stat(path1, &stb1) != 0) {
		if (!(Nflag || Pflag) || errno != ENOENT) {
			warn("%s", path1);
			return;
		}
		flags |= D_EMPTY1;
		memset(&stb1, 0, sizeof(stb1));
	}

	strlcpy(path2 + plen2, dp->d_name, MAXPATHLEN - plen2);
	if (stat(path2, &stb2) != 0) {
		if (!Nflag || errno != ENOENT) {
			warn("%s", path2);
			return;
		}
		flags |= D_EMPTY2;
		memset(&stb2, 0, sizeof(stb2));
		stb2.st_mode = stb1.st_mode;
	}
	if (stb1.st_mode == 0)
		stb1.st_mode = stb2.st_mode;

	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
		if (rflag)
			diffdir(path1, path2, flags);
		else if (lflag)
			dp->d_status |= D_COMMON;
		else
			printf("Common subdirectories: %s and %s\n",
			    path1, path2);
		return;
	}
	if (!S_ISREG(stb1.st_mode) && !S_ISDIR(stb1.st_mode))
		dp->d_status = D_SKIPPED1;
	else if (!S_ISREG(stb2.st_mode) && !S_ISDIR(stb2.st_mode))
		dp->d_status = D_SKIPPED2;
	else
		dp->d_status = diffreg(path1, path2, flags);
	if (!lflag)
		print_status(dp->d_status, path1, path2, NULL);
}

/*
 * Exclude the given directory entry?
 */
static int
excluded(const char *entry)
{
	struct excludes *excl;

	/* always skip "." and ".." */
	if (entry[0] == '.' &&
	    (entry[1] == '\0' || (entry[1] == '.' && entry[2] == '\0')))
		return (1);

	/* check excludes list */
	for (excl = excludes_list; excl != NULL; excl = excl->next)
		if (fnmatch(excl->pattern, entry, FNM_PATHNAME) == 0)
			return (1);

	return (0);
}

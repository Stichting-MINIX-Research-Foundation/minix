/* fsck_ffs - read-only consistency checker for UFS2 (FFS) file systems.
 *
 * This is a focused, non-repairing checker.  It validates the superblock
 * geometry, then for every cylinder group recomputes the free-block, free-
 * fragment and free-inode counts directly from the on-disk bitmaps and
 * cross-checks them against the cylinder-group summary (cg_cs), the in-core
 * summary array (fs_cs), and the global totals (fs_cstotal).  It also sanity-
 * checks the root inode.
 *
 * It does NOT repair anything; it reports inconsistencies and exits non-zero
 * if any are found (like "fsck -n").  Native little-endian UFS1 and UFS2.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <stdarg.h>

#include "ufs2_format.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

static int fd;
static struct fs sb;
static int nerr;

static void problem(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "fsck_ffs: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	nerr++;
}

static void rd(void *buf, off_t off, size_t n, const char *what)
{
	if (pread(fd, buf, n, off) != (ssize_t)n)
		err(1, "read %s", what);
}

static int isblock(struct fs *fs, u_char *cp, int blkno)
{
	u_char mask;
	switch ((int)fs->fs_frag) {
	case 8: return cp[blkno] == 0xff;
	case 4: mask = 0x0f << ((blkno & 1) << 2); return (cp[blkno>>1]&mask)==mask;
	case 2: mask = 0x03 << ((blkno & 3) << 1); return (cp[blkno>>2]&mask)==mask;
	case 1: mask = 0x01 << (blkno & 7); return (cp[blkno>>3]&mask)==mask;
	}
	return 0;
}

int main(int argc, char **argv)
{
	static const off_t search[] = { SBLOCK_UFS2, SBLOCK_UFS1, 0, 262144, -1 };
	struct csum total, *fscs;
	struct ufs2_dinode root;
	char *cgbuf;
	off_t sboff = -1;
	size_t dinosize;
	int i, found = 0, is_ufs1 = 0;
	uint32_t cg;

	if (argc != 2) {
		fprintf(stderr, "usage: fsck_ffs special\n");
		return 8;
	}
	if ((fd = open(argv[1], O_RDONLY)) < 0)
		err(8, "open %s", argv[1]);

	/* Locate and validate the superblock. */
	for (i = 0; search[i] != -1; i++) {
		rd(&sb, search[i], sizeof(sb), "superblock");
		if ((sb.fs_magic == FS_UFS2_MAGIC || sb.fs_magic == FS_UFS2EA_MAGIC ||
		    sb.fs_magic == FS_UFS1_MAGIC) &&
		    sb.fs_sblockloc == search[i]) {
			sboff = search[i];
			found = 1;
			break;
		}
	}
	if (!found) {
		problem("no UFS1/UFS2 superblock found");
		return 8;
	}
	is_ufs1 = (sb.fs_magic == FS_UFS1_MAGIC);
	dinosize = is_ufs1 ? DINODE1_SIZE : DINODE2_SIZE;

	/* UFS1 keeps the cg-summary address and the global free-count totals in
	 * narrow "fs_old_*" slots; copy them up into the fields we check. */
	if (is_ufs1) {
		sb.fs_csaddr = sb.fs_old_csaddr;
		sb.fs_cstotal.cs_ndir = sb.fs_old_cstotal.cs_ndir;
		sb.fs_cstotal.cs_nbfree = sb.fs_old_cstotal.cs_nbfree;
		sb.fs_cstotal.cs_nifree = sb.fs_old_cstotal.cs_nifree;
		sb.fs_cstotal.cs_nffree = sb.fs_old_cstotal.cs_nffree;
	}
	printf("** superblock at byte %lld (magic 0x%x, %s)\n",
	    (long long)sboff, sb.fs_magic, is_ufs1 ? "UFS1" : "UFS2");

	if (sb.fs_bsize <= 0 || sb.fs_fsize <= 0 ||
	    sb.fs_bsize / sb.fs_fsize != sb.fs_frag)
		problem("inconsistent block/frag/fragcount geometry");
	if (sb.fs_inopb != (u_int32_t)(sb.fs_bsize / (int)dinosize))
		problem("inconsistent inopb");
	if (sb.fs_ncg < 1 || sb.fs_ipg < 1)
		problem("degenerate cylinder-group geometry");
	printf("** %u cylinder groups, %u inodes/group, bsize %d fsize %d\n",
	    sb.fs_ncg, sb.fs_ipg, sb.fs_bsize, sb.fs_fsize);

	/* Read the cylinder-group summary array. */
	fscs = malloc(sb.fs_cssize);
	if (fscs == NULL)
		err(8, "malloc");
	rd(fscs, (off_t)sb.fs_csaddr * sb.fs_fsize, sb.fs_cssize, "cg summary");

	cgbuf = malloc(sb.fs_cgsize);
	if (cgbuf == NULL)
		err(8, "malloc");

	memset(&total, 0, sizeof(total));

	/* Check each cylinder group. */
	for (cg = 0; cg < sb.fs_ncg; cg++) {
		struct cg *cgp = (struct cg *)cgbuf;
		u_char *inosused, *blksfree;
		int64_t nbfree = 0, nffree = 0, nifree = 0;
		uint32_t b, f, nblk;

		rd(cgbuf, (off_t)cgtod(&sb, cg) * sb.fs_fsize, sb.fs_cgsize, "cg");

		if (!cg_chkmagic(cgp)) {
			problem("cg %u: bad magic 0x%x", cg, cgp->cg_magic);
			continue;
		}
		if (cgp->cg_cgx != cg)
			problem("cg %u: wrong cg_cgx %u", cg, cgp->cg_cgx);

		inosused = cg_inosused(cgp);
		blksfree = cg_blksfree(cgp);

		/* recompute free inodes */
		for (b = 0; b < sb.fs_ipg; b++)
			if (!isset(inosused, b))
				nifree++;

		/* recompute free blocks and fragments */
		nblk = cgp->cg_ndblk >> sb.fs_fragshift;
		for (b = 0; b < nblk; b++) {
			if (isblock(&sb, blksfree, b)) {
				nbfree++;
			} else {
				for (f = 0; f < (uint32_t)sb.fs_frag; f++)
					if (isset(blksfree, b * sb.fs_frag + f))
						nffree++;
			}
		}
		/* trailing fragments past the last whole block */
		for (f = nblk * sb.fs_frag; f < cgp->cg_ndblk; f++)
			if (isset(blksfree, f))
				nffree++;

		if (nbfree != cgp->cg_cs.cs_nbfree)
			problem("cg %u: free-block count %lld != bitmap %lld",
			    cg, (long long)cgp->cg_cs.cs_nbfree, (long long)nbfree);
		if (nffree != cgp->cg_cs.cs_nffree)
			problem("cg %u: free-frag count %lld != bitmap %lld",
			    cg, (long long)cgp->cg_cs.cs_nffree, (long long)nffree);
		if (nifree != cgp->cg_cs.cs_nifree)
			problem("cg %u: free-inode count %lld != bitmap %lld",
			    cg, (long long)cgp->cg_cs.cs_nifree, (long long)nifree);

		/* cg_cs must match the global summary array entry */
		if (cgp->cg_cs.cs_nbfree != fscs[cg].cs_nbfree ||
		    cgp->cg_cs.cs_nffree != fscs[cg].cs_nffree ||
		    cgp->cg_cs.cs_nifree != fscs[cg].cs_nifree ||
		    cgp->cg_cs.cs_ndir   != fscs[cg].cs_ndir)
			problem("cg %u: cg_cs disagrees with fs_cs summary", cg);

		total.cs_nbfree += cgp->cg_cs.cs_nbfree;
		total.cs_nffree += cgp->cg_cs.cs_nffree;
		total.cs_nifree += cgp->cg_cs.cs_nifree;
		total.cs_ndir   += cgp->cg_cs.cs_ndir;
	}

	/* Totals must match fs_cstotal. */
	if (total.cs_nbfree != sb.fs_cstotal.cs_nbfree)
		problem("summed free blocks %lld != fs_cstotal %lld",
		    (long long)total.cs_nbfree, (long long)sb.fs_cstotal.cs_nbfree);
	if (total.cs_nffree != sb.fs_cstotal.cs_nffree)
		problem("summed free frags %lld != fs_cstotal %lld",
		    (long long)total.cs_nffree, (long long)sb.fs_cstotal.cs_nffree);
	if (total.cs_nifree != sb.fs_cstotal.cs_nifree)
		problem("summed free inodes %lld != fs_cstotal %lld",
		    (long long)total.cs_nifree, (long long)sb.fs_cstotal.cs_nifree);
	if (total.cs_ndir != sb.fs_cstotal.cs_ndir)
		problem("summed dir count %lld != fs_cstotal %lld",
		    (long long)total.cs_ndir, (long long)sb.fs_cstotal.cs_ndir);

	/* Root inode sanity. */
	{
		off_t pos = (off_t)ino_to_fsba(&sb, UFS_ROOTINO) * sb.fs_fsize +
		    (off_t)ino_to_fsbo(&sb, UFS_ROOTINO) * dinosize;
		/* di_mode/di_nlink are at the same offsets in both dinode
		 * formats, so only 'dinosize' (the inode stride) differs. */
		rd(&root, pos, dinosize, "root inode");
		if ((root.di_mode & IFMT) != IFDIR)
			problem("root inode is not a directory (mode 0%o)",
			    root.di_mode);
		if (root.di_nlink < 2)
			problem("root inode link count %d < 2", root.di_nlink);
	}

	free(fscs);
	free(cgbuf);

	if (nerr == 0) {
		printf("** FILE SYSTEM IS CLEAN\n");
		return 0;
	}
	printf("** %d INCONSISTENC%s FOUND\n", nerr, nerr == 1 ? "Y" : "IES");
	return 4;
}

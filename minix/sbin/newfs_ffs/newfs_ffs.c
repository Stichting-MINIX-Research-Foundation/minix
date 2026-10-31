/* newfs_ffs - create a UFS2 (FFS) file system.
 *
 * A focused port of the geometry/superblock/cylinder-group logic from
 * sys/ufs/ffs (via usr.sbin/makefs/ffs/mkfs.c), restricted to native
 * little-endian UFS2 with no rotational/cluster optimization
 * (fs_contigsumsize == 0).  Unlike makefs, it writes directly to the target
 * device/file and creates the root directory itself.
 *
 * The produced file system is read and written by the MINIX FFS server
 * (minix/fs/ffs) and is laid out to be fsck-clean.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <err.h>

#include "ufs2_format.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#ifndef DEV_BSHIFT
#define DEV_BSHIFT 9
#endif
#ifndef FS_OPTTIME
#define FS_OPTTIME 0
#endif
#ifndef AVFILESIZE
#define AVFILESIZE 16384
#endif
#ifndef AFPDIR
#define AFPDIR 64
#endif

#define DFL_FSIZE	4096		/* default fragment size */
#define DFL_BSIZE	32768		/* default block size */
#define DFL_MINFREE	5
#define DFL_DENSITY	16384		/* bytes per inode */
#define ROOTMODE	(IFDIR | 0755)

#define FFS_INOPF(fs)	((fs)->fs_inopb >> (fs)->fs_fragshift)

static struct fs sblock;		/* the superblock being built */
static int fd;				/* target device/file */
static long sectorsize = 512;
static struct csum *fscs;		/* cylinder-group summaries */
static char *cgbuf;			/* one cylinder group, fs_cgsize bytes */

static char *ecalloc_block(void);

/* ---- low-level I/O, addressed in DEV_BSIZE sectors like ffs_wtfs ---- */
static void wtfs(daddr_t bno, size_t size, const void *buf)
{
	if (pwrite(fd, buf, size, (off_t)bno * sectorsize) != (ssize_t)size)
		err(1, "write at sector %lld", (long long)bno);
}

/* clear (allocate) the whole block 'blkno' in the frag bitmap (set bits 0) */
static void clrblock(struct fs *fs, u_char *cp, int blkno)
{
	switch ((int)fs->fs_frag) {
	case 8: cp[blkno] = 0; break;
	case 4: cp[blkno >> 1] &= ~(0x0f << ((blkno & 1) << 2)); break;
	case 2: cp[blkno >> 2] &= ~(0x03 << ((blkno & 3) << 1)); break;
	case 1: cp[blkno >> 3] &= ~(0x01 << (blkno & 7)); break;
	}
}
static void setblock(struct fs *fs, u_char *cp, int blkno)
{
	switch ((int)fs->fs_frag) {
	case 8: cp[blkno] = 0xff; break;
	case 4: cp[blkno >> 1] |= (0x0f << ((blkno & 1) << 2)); break;
	case 2: cp[blkno >> 2] |= (0x03 << ((blkno & 3) << 1)); break;
	case 1: cp[blkno >> 3] |= (0x01 << (blkno & 7)); break;
	}
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

/* in-core CGSIZE for UFS2 with no cluster summary */
#define MY_CGSIZE(fs) \
	(sizeof(struct cg) + sizeof(int32_t) + \
	 howmany((fs)->fs_ipg, CHAR_BIT) + howmany((fs)->fs_fpg, CHAR_BIT))

static int ilog2(int v)
{
	int n;
	for (n = 0; (1 << n) < v; n++)
		;
	return n;
}

/*
 * Initialize cylinder group 'cylno': write its cg map, the duplicate super
 * block, and the (lazily-needed) initial inode blocks.  Accumulates the cg
 * summary into fscs[cylno].
 */
static void initcg(uint32_t cylno, time_t utime)
{
	struct cg *acg = (struct cg *)cgbuf;
	daddr_t cbase, dmax;
	uint32_t i, d, dlower, dupper, blkno, start;
	struct ufs2_dinode *dp;
	char *ibuf;
	int nied;

	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size)
		dmax = sblock.fs_size;
	dlower = cgsblock(&sblock, cylno) - cbase;
	dupper = cgdmin(&sblock, cylno) - cbase;
	if (cylno == 0)
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);

	memset(cgbuf, 0, sblock.fs_cgsize);
	acg->cg_time = utime;
	acg->cg_magic = CG_MAGIC;
	acg->cg_cgx = cylno;
	acg->cg_niblk = sblock.fs_ipg;
	acg->cg_initediblk = sblock.fs_ipg < 2 * FFS_INOPB(&sblock) ?
	    sblock.fs_ipg : 2 * FFS_INOPB(&sblock);
	acg->cg_ndblk = dmax - cbase;

	start = (uint32_t)((u_char *)&acg->cg_space[0] - (u_char *)acg);
	acg->cg_iusedoff = start;
	acg->cg_freeoff = acg->cg_iusedoff + howmany(sblock.fs_ipg, CHAR_BIT);
	acg->cg_nextfreeoff = acg->cg_freeoff + howmany(sblock.fs_fpg, CHAR_BIT);

	acg->cg_cs.cs_nifree += sblock.fs_ipg;
	if (cylno == 0) {
		uint32_t r;
		for (r = 0; r < UFS_ROOTINO; r++) {
			setbit(cg_inosused(acg), r);
			acg->cg_cs.cs_nifree--;
		}
	}
	/* free the data blocks below the cg's own metadata (cylno>0 only) */
	if (cylno > 0) {
		for (d = 0, blkno = 0; d < dlower;) {
			setblock(&sblock, cg_blksfree(acg), blkno);
			acg->cg_cs.cs_nbfree++;
			d += sblock.fs_frag;
			blkno++;
		}
	}
	/* leading partial block of the data area */
	if ((i = (dupper & (sblock.fs_frag - 1))) != 0) {
		acg->cg_frsum[sblock.fs_frag - i]++;
		for (d = dupper + sblock.fs_frag - i; dupper < d; dupper++) {
			setbit(cg_blksfree(acg), dupper);
			acg->cg_cs.cs_nffree++;
		}
	}
	/* whole free blocks */
	for (d = dupper, blkno = dupper >> sblock.fs_fragshift;
	     d + sblock.fs_frag <= acg->cg_ndblk;) {
		setblock(&sblock, cg_blksfree(acg), blkno);
		acg->cg_cs.cs_nbfree++;
		d += sblock.fs_frag;
		blkno++;
	}
	/* trailing partial block */
	if (d < acg->cg_ndblk) {
		acg->cg_frsum[acg->cg_ndblk - d]++;
		for (; d < acg->cg_ndblk; d++) {
			setbit(cg_blksfree(acg), d);
			acg->cg_cs.cs_nffree++;
		}
	}

	fscs[cylno] = acg->cg_cs;

	/* Write the cg map (the duplicate superblocks are written later by
	 * write_superblock once the summary totals are final). */
	wtfs(FFS_FSBTODB(&sblock, cgtod(&sblock, cylno)), sblock.fs_cgsize, cgbuf);

	/* Zero the inodes that the server expects to be pre-initialized. */
	nied = acg->cg_initediblk;
	ibuf = ecalloc_block();
	dp = (struct ufs2_dinode *)ibuf;
	for (i = 0; i < (uint32_t)FFS_INOPB(&sblock); i++)
		dp[i].di_gen = (uint32_t)random();
	for (i = 0; i < (uint32_t)nied; i += FFS_INOPB(&sblock)) {
		/* refresh random gens per block */
		for (d = 0; d < (uint32_t)FFS_INOPB(&sblock); d++)
			dp[d].di_gen = (uint32_t)random();
		wtfs(FFS_FSBTODB(&sblock, cgimin(&sblock, cylno) +
		    ffs_blkstofrags(&sblock, i / FFS_INOPB(&sblock))),
		    sblock.fs_bsize, ibuf);
	}
	free(ibuf);
}

/* allocate a zeroed fs_bsize buffer */
static char *ecalloc_block(void)
{
	char *p = calloc(1, sblock.fs_bsize);
	if (p == NULL)
		err(1, "calloc");
	return p;
}

#ifndef FS_FLAGS_UPDATED
#define FS_FLAGS_UPDATED 0x80
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif

/*
 * Create the root directory (inode UFS_ROOTINO) in cylinder group 0.
 */
static void fsinit(time_t utime)
{
	struct cg *acg = (struct cg *)cgbuf;
	struct ufs2_dinode rin;
	char *db, *iblk;
	daddr_t dblk;
	off_t off;
	int bb, nblk;
	unsigned int c;
	struct direct *d;

	/* Re-read cylinder group 0 (just written by initcg). */
	if (pread(fd, cgbuf, sblock.fs_cgsize,
	    (off_t)FFS_FSBTODB(&sblock, cgtod(&sblock, 0)) * sectorsize)
	    != (ssize_t)sblock.fs_cgsize)
		err(1, "read cg0");

	/* Allocate the root inode. */
	setbit(cg_inosused(acg), UFS_ROOTINO);
	acg->cg_cs.cs_nifree--;
	acg->cg_cs.cs_ndir++;
	sblock.fs_cstotal.cs_nifree--;
	sblock.fs_cstotal.cs_ndir++;

	/* Allocate one whole block for the root directory's data. */
	nblk = acg->cg_ndblk >> sblock.fs_fragshift;
	for (bb = 0; bb < nblk; bb++)
		if (isblock(&sblock, cg_blksfree(acg), bb))
			break;
	if (bb >= nblk)
		errx(1, "no free block for root directory");
	clrblock(&sblock, cg_blksfree(acg), bb);
	acg->cg_cs.cs_nbfree--;
	sblock.fs_cstotal.cs_nbfree--;
	dblk = cgbase(&sblock, 0) + (daddr_t)bb * sblock.fs_frag;

	fscs[0] = acg->cg_cs;

	/* Write cylinder group 0 back. */
	wtfs(FFS_FSBTODB(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize, cgbuf);

	/* Build the root directory block: "." and ".." in the first
	 * UFS_DIRBLKSIZ chunk, the remaining chunks empty. */
	db = ecalloc_block();
	d = (struct direct *)db;
	d->d_fileno = UFS_ROOTINO;
	d->d_reclen = UFS_DIRECTSIZ(1);
	d->d_type = DT_DIR;
	d->d_namlen = 1;
	d->d_name[0] = '.';
	d = (struct direct *)(db + UFS_DIRECTSIZ(1));
	d->d_fileno = UFS_ROOTINO;
	d->d_reclen = UFS_DIRBLKSIZ - UFS_DIRECTSIZ(1);
	d->d_type = DT_DIR;
	d->d_namlen = 2;
	d->d_name[0] = '.';
	d->d_name[1] = '.';
	for (c = UFS_DIRBLKSIZ; c < (unsigned int)sblock.fs_bsize;
	    c += UFS_DIRBLKSIZ) {
		d = (struct direct *)(db + c);
		d->d_fileno = 0;
		d->d_reclen = UFS_DIRBLKSIZ;
	}
	wtfs(FFS_FSBTODB(&sblock, dblk), sblock.fs_bsize, db);
	free(db);

	/* Build and write the root inode. */
	memset(&rin, 0, sizeof(rin));
	rin.di_mode = ROOTMODE;
	rin.di_nlink = 2;
	rin.di_size = sblock.fs_bsize;
	rin.di_db[0] = dblk;
	rin.di_blocks = btodb(sblock.fs_bsize);
	rin.di_atime = rin.di_mtime = rin.di_ctime = utime;
	rin.di_birthtime = utime;
	rin.di_gen = (uint32_t)random();

	iblk = ecalloc_block();
	off = (off_t)FFS_FSBTODB(&sblock, ino_to_fsba(&sblock, UFS_ROOTINO)) *
	    sectorsize;
	if (pread(fd, iblk, sblock.fs_bsize, off) != (ssize_t)sblock.fs_bsize)
		err(1, "read root inode block");
	memcpy(iblk + ino_to_fsbo(&sblock, UFS_ROOTINO) * DINODE2_SIZE,
	    &rin, sizeof(rin));
	if (pwrite(fd, iblk, sblock.fs_bsize, off) != (ssize_t)sblock.fs_bsize)
		err(1, "write root inode block");
	free(iblk);
}

static void write_superblock(void)
{
	static char sbbuf[SBLOCKSIZE];
	struct fs *copy = (struct fs *)sbbuf;
	uint32_t cylno;
	int blks, i, size;

	/* On-disk image of the superblock with in-core-only pointers cleared. */
	memset(sbbuf, 0, sizeof(sbbuf));
	memcpy(sbbuf, &sblock, sizeof(struct fs));
	memset(copy->fs_ocsp, 0, sizeof(copy->fs_ocsp));
	copy->fs_contigdirs = NULL;
	copy->fs_csp = NULL;
	copy->fs_maxcluster = NULL;
	copy->fs_active = NULL;
	copy->fs_fmod = 0;
	copy->fs_clean = FS_ISCLEAN;

	/* Primary superblock and one duplicate per cylinder group. */
	wtfs(sblock.fs_sblockloc / sectorsize, SBLOCKSIZE, sbbuf);
	for (cylno = 0; cylno < sblock.fs_ncg; cylno++)
		wtfs(FFS_FSBTODB(&sblock, cgsblock(&sblock, cylno)),
		    SBLOCKSIZE, sbbuf);

	/* Cylinder-group summaries. */
	blks = howmany(sblock.fs_cssize, sblock.fs_fsize);
	for (i = 0; i < blks; i += sblock.fs_frag) {
		size = sblock.fs_bsize;
		if (i + sblock.fs_frag > blks)
			size = (blks - i) * sblock.fs_fsize;
		wtfs(FFS_FSBTODB(&sblock, sblock.fs_csaddr + i), size,
		    (char *)fscs + i * sblock.fs_fsize);
	}
}

static int g_bsize = DFL_BSIZE, g_fsize = DFL_FSIZE;
static int g_minfree = DFL_MINFREE, g_density = DFL_DENSITY;

static void mkfs(off_t devsectors)
{
	int fragsperinode, minfpg, optimalfpg, lastminfpg, origdensity, density;
	long long sizepb;
	int32_t csfrags;
	uint32_t cylno, i;
	time_t utime = time(NULL);

	srandom((unsigned)(utime ^ getpid()));

	memset(&sblock, 0, sizeof(sblock));
	sblock.fs_magic = FS_UFS2_MAGIC;
	sblock.fs_sblockloc = SBLOCK_UFS2;
	sblock.fs_old_flags = FS_FLAGS_UPDATED;
	sblock.fs_flags = 0;
	sblock.fs_bsize = g_bsize;
	sblock.fs_fsize = g_fsize;
	sblock.fs_maxbsize = g_bsize;		/* no extents */
	sblock.fs_maxcontig = 1;		/* no clustering */
	sblock.fs_contigsumsize = 0;

	sblock.fs_bmask = ~(sblock.fs_bsize - 1);
	sblock.fs_fmask = ~(sblock.fs_fsize - 1);
	sblock.fs_qbmask = ~(int64_t)sblock.fs_bmask;
	sblock.fs_qfmask = ~(int64_t)sblock.fs_fmask;
	sblock.fs_bshift = ilog2(sblock.fs_bsize);
	sblock.fs_fshift = ilog2(sblock.fs_fsize);
	sblock.fs_frag = ffs_numfrags(&sblock, sblock.fs_bsize);
	sblock.fs_fragshift = ilog2(sblock.fs_frag);
	if (sblock.fs_frag > MAXFRAG)
		errx(1, "fragment size %d too small for block size %d",
		    sblock.fs_fsize, sblock.fs_bsize);
	sblock.fs_fsbtodb = ilog2(sblock.fs_fsize / (int)sectorsize);
	sblock.fs_size = FFS_DBTOFSB(&sblock, devsectors);

	sblock.fs_nindir = sblock.fs_bsize / sizeof(int64_t);
	sblock.fs_inopb = sblock.fs_bsize / sizeof(struct ufs2_dinode);
	sblock.fs_maxsymlinklen = (UFS_NDADDR + UFS_NIADDR) * sizeof(int64_t);

	sblock.fs_sblkno = roundup(howmany(sblock.fs_sblockloc + SBLOCKSIZE,
	    sblock.fs_fsize), sblock.fs_frag);
	sblock.fs_cblkno = sblock.fs_sblkno +
	    roundup(howmany(SBLOCKSIZE, sblock.fs_fsize), sblock.fs_frag);
	sblock.fs_iblkno = sblock.fs_cblkno + sblock.fs_frag;
	sblock.fs_maxfilesize = (u_int64_t)sblock.fs_bsize * UFS_NDADDR - 1;
	for (sizepb = sblock.fs_bsize, i = 0; i < UFS_NIADDR; i++) {
		sizepb *= FFS_NINDIR(&sblock);
		sblock.fs_maxfilesize += sizepb;
	}

	/* Determine blocks per cylinder group (per ffs_mkfs). */
	origdensity = density = g_density;
	for (;;) {
		fragsperinode = MAX(ffs_numfrags(&sblock, density), 1);
		minfpg = fragsperinode * FFS_INOPB(&sblock);
		if (minfpg > sblock.fs_size)
			minfpg = sblock.fs_size;
		sblock.fs_ipg = FFS_INOPB(&sblock);
		sblock.fs_fpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / FFS_INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_fpg < minfpg)
			sblock.fs_fpg = minfpg;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    FFS_INOPB(&sblock));
		sblock.fs_fpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / FFS_INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_fpg < minfpg)
			sblock.fs_fpg = minfpg;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    FFS_INOPB(&sblock));
		if (MY_CGSIZE(&sblock) < (unsigned long)sblock.fs_bsize)
			break;
		density -= sblock.fs_fsize;
	}
	if (density != origdensity)
		printf("density reduced from %d to %d\n", origdensity, density);

	{ /* pack more blocks per cg */
		int maxblkspercg = sblock.fs_size - 1;
		for (; sblock.fs_fpg < maxblkspercg; sblock.fs_fpg += sblock.fs_frag) {
			sblock.fs_ipg = roundup(howmany(sblock.fs_fpg,
			    fragsperinode), FFS_INOPB(&sblock));
			if (sblock.fs_size / sblock.fs_fpg < 1)
				break;
			if (MY_CGSIZE(&sblock) < (unsigned long)sblock.fs_bsize)
				continue;
			if (MY_CGSIZE(&sblock) == (unsigned long)sblock.fs_bsize)
				break;
			sblock.fs_fpg -= sblock.fs_frag;
			sblock.fs_ipg = roundup(howmany(sblock.fs_fpg,
			    fragsperinode), FFS_INOPB(&sblock));
			break;
		}
	}

	optimalfpg = sblock.fs_fpg;
	for (;;) {
		sblock.fs_ncg = howmany(sblock.fs_size, sblock.fs_fpg);
		lastminfpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / FFS_INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_size < lastminfpg)
			errx(1, "file system size %lld < minimum %d",
			    (long long)sblock.fs_size, lastminfpg);
		if (sblock.fs_size % sblock.fs_fpg >= (uint32_t)lastminfpg ||
		    sblock.fs_size % sblock.fs_fpg == 0)
			break;
		sblock.fs_fpg -= sblock.fs_frag;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    FFS_INOPB(&sblock));
	}
	(void)optimalfpg;

	sblock.fs_cgsize = ffs_fragroundup(&sblock, MY_CGSIZE(&sblock));
	sblock.fs_dblkno = sblock.fs_iblkno + sblock.fs_ipg / FFS_INOPF(&sblock);
	sblock.fs_csaddr = cgdmin(&sblock, 0);
	sblock.fs_cssize = ffs_fragroundup(&sblock,
	    sblock.fs_ncg * sizeof(struct csum));
	sblock.fs_sbsize = ffs_fragroundup(&sblock, sizeof(struct fs));
	if (sblock.fs_sbsize > SBLOCKSIZE)
		sblock.fs_sbsize = SBLOCKSIZE;
	sblock.fs_minfree = g_minfree;
	sblock.fs_maxbpg = sblock.fs_fpg / sblock.fs_frag;
	sblock.fs_optim = FS_OPTTIME;
	sblock.fs_cgrotor = 0;
	sblock.fs_avgfilesize = AVFILESIZE;
	sblock.fs_avgfpdir = AFPDIR;
	sblock.fs_id[0] = (int32_t)utime;
	sblock.fs_id[1] = (int32_t)random();
	sblock.fs_clean = FS_ISCLEAN;
	sblock.fs_time = utime;

	csfrags = howmany(sblock.fs_cssize, sblock.fs_fsize);
	sblock.fs_dsize = sblock.fs_size - sblock.fs_sblkno -
	    sblock.fs_ncg * (sblock.fs_dblkno - sblock.fs_sblkno);
	sblock.fs_cstotal.cs_nbfree = ffs_fragstoblks(&sblock, sblock.fs_dsize) -
	    howmany(csfrags, sblock.fs_frag);
	sblock.fs_cstotal.cs_nffree = ffs_fragnum(&sblock, sblock.fs_size) +
	    (ffs_fragnum(&sblock, csfrags) > 0 ?
	    sblock.fs_frag - ffs_fragnum(&sblock, csfrags) : 0);
	sblock.fs_cstotal.cs_nifree = sblock.fs_ncg * sblock.fs_ipg - UFS_ROOTINO;
	sblock.fs_cstotal.cs_ndir = 0;
	sblock.fs_dsize -= csfrags;

	printf("%s: %.1fMB, block %d frag %d, %u cyl groups of %u inodes\n",
	    "newfs_ffs",
	    (double)sblock.fs_size * sblock.fs_fsize / (1024.0 * 1024.0),
	    sblock.fs_bsize, sblock.fs_fsize, sblock.fs_ncg, sblock.fs_ipg);

	fscs = calloc(1, sblock.fs_cssize);
	cgbuf = calloc(1, sblock.fs_cgsize);
	if (!fscs || !cgbuf)
		err(1, "calloc");

	for (cylno = 0; cylno < sblock.fs_ncg; cylno++)
		initcg(cylno, utime);

	fsinit(utime);
	write_superblock();
}

int main(int argc, char **argv)
{
	off_t size = 0;		/* in sectors */
	int ch;

	while ((ch = getopt(argc, argv, "b:f:s:m:O:")) != -1) {
		switch (ch) {
		case 'b': g_bsize = atoi(optarg); break;
		case 'f': g_fsize = atoi(optarg); break;
		case 's': size = strtoll(optarg, NULL, 0); break;
		case 'm': g_minfree = atoi(optarg); break;
		case 'O': break;	/* version: UFS2 only */
		default:
			fprintf(stderr, "usage: newfs_ffs [-b bsize] [-f fsize] "
			    "[-m minfree] [-s sectors] special\n");
			return 1;
		}
	}
	argc -= optind; argv += optind;
	if (argc != 1) {
		fprintf(stderr, "usage: newfs_ffs [-b bsize] [-f fsize] "
		    "[-m minfree] [-s sectors] special\n");
		return 1;
	}

	fd = open(argv[0], O_RDWR | O_CREAT, 0644);
	if (fd < 0)
		err(1, "open %s", argv[0]);

	if (size == 0) {
		off_t end = lseek(fd, 0, SEEK_END);
		if (end <= 0)
			errx(1, "cannot determine size of %s; use -s", argv[0]);
		size = end / sectorsize;
	} else {
		/* Ensure the backing file is large enough. */
		if (ftruncate(fd, size * sectorsize) < 0)
			err(1, "ftruncate");
	}

	if ((g_bsize & (g_bsize - 1)) || (g_fsize & (g_fsize - 1)))
		errx(1, "block and fragment sizes must be powers of two");
	if (g_bsize / g_fsize > MAXFRAG)
		errx(1, "block size / fragment size > %d", MAXFRAG);

	mkfs(size);

	if (close(fd) < 0)
		err(1, "close");
	printf("newfs_ffs: created UFS2 file system on %s\n", argv[0]);
	return 0;
}

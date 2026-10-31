#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "ffs_disk.h"

static int fd;
static struct fs sb;

static void rd(void *buf, off_t off, size_t n) {
	if (pread(fd, buf, n, off) != (ssize_t)n) { perror("pread"); exit(1); }
}
static void read_dinode(ino_t ino, struct ufs2_dinode *dp) {
	daddr_t fsba = ino_to_fsba(&sb, ino);
	off_t byteoff = (off_t)ino_to_fsbo(&sb, ino) * DINODE2_SIZE;
	rd(dp, (off_t)fsba * sb.fs_fsize + byteoff, sizeof(*dp));
}
static daddr_t bmap0(struct ufs2_dinode *dp, off_t lbn) {
	return (lbn < UFS_NDADDR) ? dp->di_db[lbn] : 0;
}

int main(int argc, char **argv) {
	setvbuf(stdout, NULL, _IONBF, 0);
	if (argc != 2) { fprintf(stderr, "usage: %s image\n", argv[0]); return 1; }
	if ((fd = open(argv[1], O_RDONLY)) < 0) { perror("open"); return 1; }

	long search[] = { 65536, 8192, 0, 262144, -1 };
	int si, found = 0;
	for (si = 0; search[si] >= 0; si++) {
		rd(&sb, search[si], sizeof(sb));
		if (sb.fs_magic == FS_UFS2_MAGIC || sb.fs_magic == FS_UFS2EA_MAGIC) {
			printf("superblock found @ byte %ld (magic 0x%x)\n", search[si], sb.fs_magic);
			found = 1; break;
		}
	}
	if (!found) { printf("no UFS2 superblock found\n"); return 1; }

	printf("bsize=%d fsize=%d frag=%d inopb=%u nindir=%d\n",
		sb.fs_bsize, sb.fs_fsize, sb.fs_frag, sb.fs_inopb, sb.fs_nindir);
	printf("ncg=%u ipg=%u fpg=%d iblkno=%d csaddr=%lld maxsymlinklen=%d\n",
		sb.fs_ncg, sb.fs_ipg, sb.fs_fpg, sb.fs_iblkno,
		(long long)sb.fs_csaddr, sb.fs_maxsymlinklen);

	struct ufs2_dinode root;
	read_dinode(UFS_ROOTINO, &root);
	printf("\nroot ino=2 mode=0%o nlink=%d size=%llu isdir=%d\n",
		root.di_mode, root.di_nlink, (unsigned long long)root.di_size,
		(root.di_mode & IFMT) == IFDIR);

	printf("\nroot directory entries:\n");
	char blk[8192];
	for (off_t p = 0; p < (off_t)root.di_size; p += UFS_DIRBLKSIZ) {
		daddr_t base = bmap0(&root, ffs_lblkno(&sb, p));
		if (base == 0) continue;
		off_t boff = ffs_blkoff(&sb, p);
		off_t frag = base + (boff >> sb.fs_fshift);
		off_t within = boff & (sb.fs_fsize - 1);
		rd(blk, frag * sb.fs_fsize + within, UFS_DIRBLKSIZ);
		unsigned o = 0;
		while (o < UFS_DIRBLKSIZ) {
			struct direct *d = (struct direct *)(blk + o);
			if (d->d_reclen < UFS_DIRECTSIZ(0)) break;
			if (d->d_ino) {
				char nm[256];
				memcpy(nm, d->d_name, d->d_namlen); nm[d->d_namlen] = 0;
				printf("  ino=%-4u type=%-2u name='%s'\n", d->d_ino, d->d_type, nm);
				if (d->d_type == 8) {
					struct ufs2_dinode f; read_dinode(d->d_ino, &f);
					daddr_t fb = bmap0(&f, 0);
					char fbuf[4097] = {0};
					size_t n = f.di_size < 4096 ? f.di_size : 4096;
					if (fb) rd(fbuf, (off_t)fb * sb.fs_fsize, n);
					fbuf[n] = 0;
					printf("       contents(size=%llu): %s", (unsigned long long)f.di_size, fbuf);
				}
				if (d->d_type == 10) {
					struct ufs2_dinode l; read_dinode(d->d_ino, &l);
					char t[200] = {0};
					if (l.di_size < sb.fs_maxsymlinklen) { memcpy(t, l.di_db, l.di_size); t[l.di_size]=0;
						printf("       symlink -> %s\n", t); }
				}
			}
			o += d->d_reclen;
		}
	}
	return 0;
}

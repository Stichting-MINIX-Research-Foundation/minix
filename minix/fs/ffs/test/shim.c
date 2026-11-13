/* Host shim: a file-backed block cache (lmfs), bdev, and fsdriver helpers, so
 * the real FFS server source files can run against an image file on Linux. */
#include <minixshim.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>

extern int g_fd;		/* image fd, set by the harness */

static size_t g_bs;		/* cache block size (fragment size) */
static struct buf **g_cache;	/* indexed by block number */
static size_t g_ncache;

void panic(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fprintf(stderr, "\n*** PANIC: "); vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n"); va_end(ap); abort();
}
time_t clock_time(clock_t *p) { (void)p; return time(NULL); }

/* ---- bdev: direct pread/pwrite on the image ---- */
int bdev_open(dev_t dev, int access) { (void)dev; (void)access; return OK; }
int bdev_close(dev_t dev) { (void)dev; return OK; }
ssize_t bdev_read(dev_t dev, uint64_t pos, char *buf, size_t count, int flags) {
	(void)dev; (void)flags;
	ssize_t r = pread(g_fd, buf, count, (off_t)pos);
	return r;
}
ssize_t bdev_write(dev_t dev, uint64_t pos, char *buf, size_t count, int flags) {
	(void)dev; (void)flags;
	return pwrite(g_fd, buf, count, (off_t)pos);
}

/* ---- lmfs block cache ---- */
void lmfs_set_blocksize(size_t bs) {
	off_t sz;
	g_bs = bs;
	sz = lseek(g_fd, 0, SEEK_END);
	if (sz < 0) panic("lseek");
	g_ncache = (size_t)(sz / bs) + 16;
	g_cache = calloc(g_ncache, sizeof(*g_cache));
	if (!g_cache) panic("cache alloc");
}
unsigned int lmfs_fs_block_size(void) { return (unsigned int)g_bs; }
void lmfs_set_blockusage(uint64_t t, uint64_t u) { (void)t; (void)u; }
void lmfs_may_use_vmcache(int n) { (void)n; }
void lmfs_buf_pool(int n) { (void)n; }
void lmfs_invalidate(dev_t dev) { (void)dev; }
void lmfs_free_block(dev_t dev, block64_t block) { (void)dev; (void)block; }

static void flush_buf(struct buf *bp) {
	if (bp->dirty) {
		if (pwrite(g_fd, bp->data, g_bs, (off_t)bp->lmfs_blocknr * g_bs)
		    != (ssize_t)g_bs)
			panic("flush pwrite blk %llu",
			    (unsigned long long)bp->lmfs_blocknr);
		bp->dirty = 0;
	}
}

int lmfs_get_block_ino(struct buf **bpp, dev_t dev, block64_t block, int how,
	ino_t ino, uint64_t off) {
	(void)dev; (void)ino; (void)off;
	if (block >= g_ncache) panic("block %llu out of range (ncache=%zu)",
		(unsigned long long)block, g_ncache);
	struct buf *bp = g_cache[block];
	if (bp == NULL) {
		if (how == PEEK) return ENOENT;
		bp = calloc(1, sizeof(*bp));
		bp->data = malloc(g_bs);
		bp->lmfs_blocknr = block;
		if (how == NORMAL) {
			if (pread(g_fd, bp->data, g_bs, (off_t)block * g_bs)
			    != (ssize_t)g_bs)
				panic("read pread blk %llu",
				    (unsigned long long)block);
		} else {
			memset(bp->data, 0, g_bs);	/* NO_READ */
		}
		g_cache[block] = bp;
	}
	bp->refcount++;
	*bpp = bp;
	return OK;
}
int lmfs_get_block(struct buf **bpp, dev_t dev, block64_t block, int how) {
	return lmfs_get_block_ino(bpp, dev, block, how, NO_ENTRY, 0);
}
void lmfs_markdirty(struct buf *bp) { bp->dirty = 1; }
void lmfs_put_block(struct buf *bp) {
	if (bp == NULL) return;
	if (bp->refcount > 0) bp->refcount--;
	flush_buf(bp);			/* write-through so the image stays current */
}
void lmfs_zero_block_ino(dev_t dev, ino_t ino, uint64_t off) {
	(void)dev; (void)ino; (void)off;
}
void lmfs_flushall(void) {
	size_t i;
	for (i = 0; i < g_ncache; i++)
		if (g_cache[i]) flush_buf(g_cache[i]);
}

/* ---- fsdriver data copy ---- */
int fsdriver_copyin(struct fsdriver_data *d, size_t off, void *buf, size_t n) {
	memcpy(buf, d->ptr + off, n); return OK;
}
int fsdriver_copyout(struct fsdriver_data *d, size_t off, void *buf, size_t n) {
	memcpy(d->ptr + off, buf, n); return OK;
}
int fsdriver_zero(struct fsdriver_data *d, size_t off, size_t n) {
	memset(d->ptr + off, 0, n); return OK;
}

/* ---- getdents collector (results exposed to the harness) ---- */
#define MAXDENT 256
ino_t        h_dent_ino[MAXDENT];
char         h_dent_name[MAXDENT][256];
unsigned int h_dent_type[MAXDENT];
int          h_dent_count;

void fsdriver_dentry_init(struct fsdriver_dentry *dp, struct fsdriver_data *data,
	size_t bytes, char *buf, size_t bufsize) {
	dp->data = data; dp->bytes = bytes; dp->buf = buf; dp->bufsize = bufsize;
}
ssize_t fsdriver_dentry_add(struct fsdriver_dentry *dp, ino_t ino,
	const char *name, size_t namelen, unsigned int type) {
	(void)dp;
	if (h_dent_count >= MAXDENT) return 0;	/* "buffer full" */
	h_dent_ino[h_dent_count] = ino;
	memcpy(h_dent_name[h_dent_count], name, namelen);
	h_dent_name[h_dent_count][namelen] = '\0';
	h_dent_type[h_dent_count] = type;
	h_dent_count++;
	return 1;
}
ssize_t fsdriver_dentry_finish(struct fsdriver_dentry *dp) {
	(void)dp; return h_dent_count;
}


/* File that implements the data structure, insert, lookup and remove
 * functions for file system cache blocks.
 *
 * Cache blocks can be mapped into the memory of processes by the
 * 'cache' and 'file' memory types.
 */

#include <assert.h>
#include <string.h>

#include <minix/hash.h>

#include "proto.h"
#include "vm.h"
#include "region.h"
#include "glo.h"
#include "cache.h"

/* cache datastructure */
#define HASHSIZE 65536

static struct cached_page *cache_hash_bydev[HASHSIZE];
static struct cached_page *cache_hash_byino[HASHSIZE];
static struct cached_page *lru_oldest = NULL, *lru_newest = NULL;

static u32_t cached_pages = 0;

static void lru_rm(struct cached_page *hb)
{
	struct cached_page *newer = hb->newer, *older = hb->older;
	assert(lru_newest);
	assert(lru_oldest);
	if(newer) {
		assert(newer->older == hb);
		newer->older = older;
	}
	if(older) {
		assert(older->newer == hb);
		older->newer = newer;
	}

	if(lru_newest == hb) { assert(!newer); lru_newest = older; }
	if(lru_oldest == hb) { assert(!older); lru_oldest = newer; }

	if(lru_newest) assert(lru_newest->newer == NULL);
	if(lru_oldest) assert(lru_oldest->older == NULL);

	cached_pages--;
}

static void lru_add(struct cached_page *hb)
{
	if(lru_newest) {
		assert(lru_oldest);
		assert(!lru_newest->newer);
		lru_newest->newer = hb;
	} else {
		assert(!lru_oldest);
		lru_oldest = hb;
	}

	hb->older = lru_newest;
	hb->newer = NULL;
	lru_newest = hb;

	cached_pages++;
}

void cache_lru_touch(struct cached_page *hb)
{
	lru_rm(hb);
	lru_add(hb);
}

static __inline u32_t makehash(u32_t p1, u64_t p2)
{
	u32_t offlo = ex64lo(p2), offhi = ex64hi(p2),
		v = 0x12345678;
	hash_mix(p1, offlo, offhi);
	hash_final(offlo, offhi, v);

	return v % HASHSIZE;
}

#if CACHE_SANITY
void cache_sanitycheck_internal(void)
{
	int h;
	int n = 0;
	int byino = 0;
	int withino = 0;
	int bydev_total = 0, lru_total = 0;
	struct cached_page *cp;

	for(h = 0; h < HASHSIZE; h++) {
		for(cp = cache_hash_bydev[h]; cp; cp = cp->hash_next_dev) {
			assert(cp->dev != NO_DEV);
			assert(h == makehash(cp->dev, cp->dev_offset));
			assert(cp == find_cached_page_bydev(cp->dev, cp->dev_offset, cp->ino, cp->ino_offset));
			if(cp->ino != VMC_NO_INODE) withino++;
			bydev_total++;
			n++;
			assert(n < 1500000);
		}
		for(cp = cache_hash_byino[h]; cp; cp = cp->hash_next_ino) {
			assert(cp->dev != NO_DEV);
			assert(cp->ino != VMC_NO_INODE);
			assert(h == makehash(cp->ino, cp->ino_offset));
			byino++;
			n++;
			assert(n < 1500000);
		}
	}

	assert(byino == withino);

	if(lru_newest) {
		assert(lru_oldest);
		assert(!lru_newest->newer);
		assert(!lru_oldest->older);
	} else {
		assert(!lru_oldest);
	}

	for(cp = lru_oldest; cp; cp = cp->newer) {
		struct cached_page *newer = cp->newer,
			*older = cp->older;
		if(newer) assert(newer->older == cp);
		if(older) assert(older->newer == cp);
		lru_total++;
	}

	assert(lru_total == bydev_total);

	assert(lru_total == cached_pages);
}
#endif

#define rmhash_f(fname, nextfield)			\
static void						\
fname(struct cached_page *cp, struct cached_page **head)	\
{								\
	struct cached_page *hb;					\
	if(*head == cp) { *head = cp->nextfield; return; }			\
	for(hb = *head; hb && cp != hb->nextfield; hb = hb->nextfield) ; \
	assert(hb); assert(hb->nextfield == cp);		\
	hb->nextfield = cp->nextfield;		\
	return;					\
}

rmhash_f(rmhash_byino, hash_next_ino)
rmhash_f(rmhash_bydev, hash_next_dev)

static void addcache_byino(struct cached_page *hb)
{
	int hv_ino = makehash(hb->ino, hb->ino_offset);
	assert(hb->ino != VMC_NO_INODE);
	hb->hash_next_ino = cache_hash_byino[hv_ino];
	cache_hash_byino[hv_ino] = hb;
}

static void
update_inohash(struct cached_page *hb, ino_t ino, u64_t ino_off)
{
	assert(ino != VMC_NO_INODE);
	if(hb->ino != VMC_NO_INODE) {
		int h = makehash(hb->ino, hb->ino_offset);
		rmhash_byino(hb, &cache_hash_byino[h]);
	}
	hb->ino = ino;
	hb->ino_offset = ino_off;
	addcache_byino(hb);
}

struct cached_page *
find_cached_page_bydev(dev_t dev, u64_t dev_off, ino_t ino, u64_t ino_off, int touchlru)
{
	struct cached_page *hb;

	for(hb = cache_hash_bydev[makehash(dev, dev_off)]; hb; hb=hb->hash_next_dev) {
		if(hb->dev == dev && hb->dev_offset == dev_off) {
			if(ino != VMC_NO_INODE) {
				if(hb->ino != ino || hb->ino_offset != ino_off) {
					update_inohash(hb, ino, ino_off);
				}
			}

			if(touchlru) cache_lru_touch(hb);

			return hb;
		}
	}

	return NULL;
}

struct cached_page *find_cached_page_byino(dev_t dev, ino_t ino, u64_t ino_off, int touchlru)
{
	struct cached_page *hb;

	assert(ino != VMC_NO_INODE);
	assert(dev != NO_DEV);

	for(hb = cache_hash_byino[makehash(ino, ino_off)]; hb; hb=hb->hash_next_ino) {
		if(hb->dev == dev && hb->ino == ino && hb->ino_offset == ino_off) {
			if(touchlru) cache_lru_touch(hb);

			return hb;
		}
	}

	return NULL;
}

int addcache(dev_t dev, u64_t dev_off, ino_t ino, u64_t ino_off, struct phys_block *pb)
{
	int hv_dev;
        struct cached_page *hb;

	if(pb->flags & PBF_INCACHE) {
		printf("VM: already in cache\n");
		return EINVAL;
	}

        if(!SLABALLOC(hb)) {
                printf("VM: no memory for cache node\n");
                return ENOMEM;
        }

	assert(dev != NO_DEV);
#if CACHE_SANITY
	assert(!find_cached_page_bydev(dev, dev_off, ino, ino_off));
#endif

        hb->dev = dev;
        hb->dev_offset = dev_off;
        hb->ino = ino;
        hb->ino_offset = ino_off;
        hb->page = pb;
        hb->page->refcount++;   /* block also referenced by cache now */
	hb->page->flags |= PBF_INCACHE;

        hv_dev = makehash(dev, dev_off);

        hb->hash_next_dev = cache_hash_bydev[hv_dev];
        cache_hash_bydev[hv_dev] = hb;

        if(hb->ino != VMC_NO_INODE)
		addcache_byino(hb);

	lru_add(hb);

	return OK;
}

void rmcache(struct cached_page *cp)
{
	struct phys_block *pb = cp->page;
        int hv_dev = makehash(cp->dev, cp->dev_offset);

	assert(cp->page->flags & PBF_INCACHE);

	cp->page->flags &= ~PBF_INCACHE;

	rmhash_bydev(cp, &cache_hash_bydev[hv_dev]);
	if(cp->ino != VMC_NO_INODE) {
		int hv_ino = makehash(cp->ino, cp->ino_offset);
		rmhash_byino(cp, &cache_hash_byino[hv_ino]);
	}

	assert(cp->page->refcount >= 1);
	cp->page->refcount--;

	lru_rm(cp);

	if(pb->refcount == 0) {
		assert(pb->phys != MAP_NONE);
		free_mem(ABS2CLICK(pb->phys), 1);
		SLABFREE(pb);
	}

	SLABFREE(cp);
}

int cache_freepages(int pages)
{
	struct cached_page *cp, *newercp;
	int freed = 0;
	int oldsteps = 0;
	int skips = 0;

	for(cp = lru_oldest; cp && freed < pages; cp = newercp) {
		newercp = cp->newer;
		assert(cp->page->refcount >= 1);
		if(cp->page->refcount == 1) {
			rmcache(cp);
			freed++;
			skips = 0;
		} else skips++;
		oldsteps++;
	}

	return freed;
}

/*
 * Remove all pages that are associated with the given device.
 */
void
clear_cache_bydev(dev_t dev)
{
	struct cached_page *cp, *ncp;
	int h;

	for (h = 0; h < HASHSIZE; h++) {
		for (cp = cache_hash_bydev[h]; cp != NULL; cp = ncp) {
			ncp = cp->hash_next_dev;

			if (cp->dev == dev)
				rmcache(cp);
		}
	}
}

void get_stats_info(struct vm_stats_info *vsi)
{
        vsi->vsi_cached = cached_pages;
}


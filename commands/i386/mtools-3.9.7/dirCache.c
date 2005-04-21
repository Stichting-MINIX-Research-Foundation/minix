#include "sysincludes.h"
#include "vfat.h"
#include "dirCache.h"


void myfree(void *a)
{
	free(a);
}

#define free myfree


#define BITS_PER_INT (sizeof(unsigned int) * 8)


static inline unsigned int rol(unsigned int arg, int shift)
{
	arg &= 0xffffffff; /* for 64 bit machines */
	return (arg << shift) | (arg >> (32 - shift));
}


static int calcHash(char *name)
{
	unsigned long hash;
	int i;
	unsigned char c;

	hash = 0;
	i = 0;
	while(*name) {
		/* rotate it */
		hash = rol(hash,5); /* a shift of 5 makes sure we spread quickly
				     * over the whole width, moreover, 5 is
				     * prime with 32, which makes sure that
				     * successive letters cannot cover each 
				     * other easily */
		c = toupper(*name);
		hash ^=  (c * (c+2)) ^ (i * (i+2));
		hash &= 0xffffffff;
		i++, name++;
	}
	hash = hash * (hash + 2);
	/* the following two xors make sure all info is spread evenly over all
	 * bytes. Important if we only keep the low order bits later on */
	hash ^= (hash & 0xfff) << 12;
	hash ^= (hash & 0xff000) << 24;
	return hash;
}

static int addBit(unsigned int *bitmap, int hash, int checkOnly)
{
	int bit, entry;

	bit = 1 << (hash % BITS_PER_INT);
	entry = (hash / BITS_PER_INT) % DC_BITMAP_SIZE;
	
	if(checkOnly)
		return bitmap[entry] & bit;
	else {
		bitmap[entry] |= bit;
		return 1;
	}
}

static int _addHash(dirCache_t *cache, unsigned int hash, int checkOnly)
{
	return
		addBit(cache->bm0, hash, checkOnly) &&
		addBit(cache->bm1, rol(hash,12), checkOnly) &&
		addBit(cache->bm2, rol(hash,24), checkOnly);
}


static void addNameToHash(dirCache_t *cache, char *name)
{	
	_addHash(cache, calcHash(name), 0);
}

static void hashDce(dirCache_t *cache, dirCacheEntry_t *dce)
{
	if(dce->beginSlot != cache->nrHashed)
		return;
	cache->nrHashed = dce->endSlot;
	if(dce->longName)
		addNameToHash(cache, dce->longName);
	addNameToHash(cache, dce->shortName);
}

int isHashed(dirCache_t *cache, char *name)
{
	int ret;

	ret =  _addHash(cache, calcHash(name), 1);
	return ret;
}

void checkXYZ(dirCache_t *cache)
{
	if(cache->entries[2])
		printf(" at 2 = %d\n", cache->entries[2]->beginSlot);
}


int growDirCache(dirCache_t *cache, int slot)
{
	if(slot < 0) {
		fprintf(stderr, "Bad slot %d\n", slot);
		exit(1);
	}

	if( cache->nr_entries <= slot) {
		int i;
		
		cache->entries = realloc(cache->entries,
					 (slot+1) * 2 * 
					 sizeof(dirCacheEntry_t *));
		if(!cache->entries)
			return -1;
		for(i= cache->nr_entries; i < (slot+1) * 2; i++) {
			cache->entries[i] = 0;
		}
		cache->nr_entries = (slot+1) * 2;
	}
	return 0;
}

dirCache_t *allocDirCache(Stream_t *Stream, int slot)
{       
	dirCache_t **dcp;

	if(slot < 0) {
		fprintf(stderr, "Bad slot %d\n", slot);
		exit(1);
	}

	dcp = getDirCacheP(Stream);
	if(!*dcp) {
		*dcp = New(dirCache_t);
		if(!*dcp)
			return 0;
		(*dcp)->entries = NewArray((slot+1)*2+5, dirCacheEntry_t *);
		if(!(*dcp)->entries) {
			free(*dcp);
			return 0;
		}
		(*dcp)->nr_entries = (slot+1) * 2;
		memset( (*dcp)->bm0, 0, DC_BITMAP_SIZE);
		memset( (*dcp)->bm1, 0, DC_BITMAP_SIZE);
		memset( (*dcp)->bm2, 0, DC_BITMAP_SIZE);
		(*dcp)->nrHashed = 0;
	} else
		if(growDirCache(*dcp, slot) < 0)
			return 0;
	return *dcp;
}

static void freeDirCacheRange(dirCache_t *cache, int beginSlot, int endSlot)
{
	dirCacheEntry_t *entry;
	int clearBegin;
	int clearEnd;
	int i;

	if(endSlot < beginSlot) {
		fprintf(stderr, "Bad slots %d %d in free range\n", 
			beginSlot, endSlot);
		exit(1);
	}

	while(beginSlot < endSlot) {
		entry = cache->entries[beginSlot];
		if(!entry) {
			beginSlot++;
			continue;
		}
		
		clearEnd = entry->endSlot;
		if(clearEnd > endSlot)
			clearEnd = endSlot;
		clearBegin = beginSlot;
		
		for(i = clearBegin; i <clearEnd; i++)
			cache->entries[i] = 0;

		if(entry->endSlot == endSlot)
			entry->endSlot = beginSlot;
		else if(entry->beginSlot == beginSlot)
			entry->beginSlot = endSlot;
		else {
			fprintf(stderr, 
				"Internal error, non contiguous de-allocation\n");
			fprintf(stderr, "%d %d\n", beginSlot, endSlot);
			fprintf(stderr, "%d %d\n", entry->beginSlot, 
				entry->endSlot);
			exit(1);			
		}

		if(entry->beginSlot == entry->endSlot) {
			if(entry->longName)
				free(entry->longName);
			if(entry->shortName)
				free(entry->shortName);
			free(entry);
		}

		beginSlot = clearEnd;
	}
}

static dirCacheEntry_t *allocDirCacheEntry(dirCache_t *cache, int beginSlot, 
					   int endSlot,
					   dirCacheEntryType_t type)
{
	dirCacheEntry_t *entry;
	int i;

	if(growDirCache(cache, endSlot) < 0)
		return 0;

	entry = New(dirCacheEntry_t);
	if(!entry)
		return 0;
	entry->type = type;
	entry->longName = 0;
	entry->shortName = 0;
	entry->beginSlot = beginSlot;
	entry->endSlot = endSlot;

	freeDirCacheRange(cache, beginSlot, endSlot);
	for(i=beginSlot; i<endSlot; i++) {
		cache->entries[i] = entry;
	}
	return entry;
}

dirCacheEntry_t *addUsedEntry(dirCache_t *cache, int beginSlot, int endSlot, 
			      char *longName, char *shortName,
			      struct directory *dir)
{
	dirCacheEntry_t *entry;

	if(endSlot < beginSlot) {
		fprintf(stderr, 
			"Bad slots %d %d in add used entry\n", 
			beginSlot, endSlot);
		exit(1);
	}


	entry = allocDirCacheEntry(cache, beginSlot, endSlot, DCET_USED);
	if(!entry)
		return 0;
	
	entry->beginSlot = beginSlot;
	entry->endSlot = endSlot;
	if(longName)
		entry->longName = strdup(longName);
	entry->shortName = strdup(shortName);
	entry->dir = *dir;
	hashDce(cache, entry);
	return entry;
}

static void mergeFreeSlots(dirCache_t *cache, int slot)
{
	dirCacheEntry_t *previous, *next;
	int i;

	if(slot == 0)
		return;
	previous = cache->entries[slot-1];
	next = cache->entries[slot];
	if(next && next->type == DCET_FREE &&
	   previous && previous->type == DCET_FREE) {
		for(i=next->beginSlot; i < next->endSlot; i++)
			cache->entries[i] = previous;
		previous->endSlot = next->endSlot;
		free(next);		
	}
}

dirCacheEntry_t *addFreeEntry(dirCache_t *cache, int beginSlot, int endSlot)
{
	dirCacheEntry_t *entry;

	if(beginSlot < cache->nrHashed)
		cache->nrHashed = beginSlot;

	if(endSlot < beginSlot) {
		fprintf(stderr, "Bad slots %d %d in add free entry\n", 
			beginSlot, endSlot);
		exit(1);
	}

	if(endSlot == beginSlot)
		return 0;
	entry = allocDirCacheEntry(cache, beginSlot, endSlot, DCET_FREE);
	mergeFreeSlots(cache, beginSlot);
	mergeFreeSlots(cache, endSlot);
	return cache->entries[beginSlot];
}


dirCacheEntry_t *addEndEntry(dirCache_t *cache, int pos)
{
	return allocDirCacheEntry(cache, pos, pos+1, DCET_END);
}

dirCacheEntry_t *lookupInDircache(dirCache_t *cache, int pos)
{
	if(growDirCache(cache, pos+1) < 0)
		return 0;
	return cache->entries[pos];	
}

void freeDirCache(Stream_t *Stream)
{
	dirCache_t *cache, **dcp;

	dcp = getDirCacheP(Stream);
	cache = *dcp;
	if(cache) {
		freeDirCacheRange(cache, 0, cache->nr_entries);
		free(cache);
		*dcp = 0;
	}
}

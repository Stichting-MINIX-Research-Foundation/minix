#ifndef MTOOLS_DIRCACHE_H
#define MTOOLS_DIRCACHE_H

typedef enum {
	DCET_FREE,
	DCET_USED,
	DCET_END
} dirCacheEntryType_t;

#define DC_BITMAP_SIZE 128

typedef struct dirCacheEntry_t {
	dirCacheEntryType_t type;
	int beginSlot;
	int endSlot;
	char *shortName;
	char *longName;
	struct directory dir;
} dirCacheEntry_t;

typedef struct dirCache_t {
	struct dirCacheEntry_t **entries;
	int nr_entries;
	unsigned int nrHashed;
	unsigned int bm0[DC_BITMAP_SIZE];
	unsigned int bm1[DC_BITMAP_SIZE];
	unsigned int bm2[DC_BITMAP_SIZE];
} dirCache_t;

int isHashed(dirCache_t *cache, char *name);
int growDirCache(dirCache_t *cache, int slot);
dirCache_t *allocDirCache(Stream_t *Stream, int slot);
dirCacheEntry_t *addUsedEntry(dirCache_t *Stream, int begin, int end, 
			      char *longName, char *shortName,
			      struct directory *dir);
void freeDirCache(Stream_t *Stream);
dirCacheEntry_t *addFreeEntry(dirCache_t *Stream, int begin, int end);
dirCacheEntry_t *addEndEntry(dirCache_t *Stream, int pos);
dirCacheEntry_t *lookupInDircache(dirCache_t *Stream, int pos);
#endif

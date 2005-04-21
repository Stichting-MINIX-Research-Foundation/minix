/* vfat.c
 *
 * Miscellaneous VFAT-related functions
 */

#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "vfat.h"
#include "file.h"
#include "dirCache.h"

/* #define DEBUG */

const char *short_illegals=";+=[]',\"*\\<>/?:|";
const char *long_illegals = "\"*\\<>/?:|\005";

/* Automatically derive a new name */
static void autorename(char *name,
		       char tilda, char dot, const char *illegals,
		       int limit, int bump)
{
	int tildapos, dotpos;
	unsigned int seqnum=0, maxseq=0;
	char tmp;
	char *p;
	
#ifdef DEBUG
	printf("In autorename for name=%s.\n", name);
#endif
	tildapos = -1;

	for(p=name; *p ; p++)
		if((*p < ' ' && *p != '\005') || strchr(illegals, *p)) {
			*p = '_';
			bump = 0;
		}

	for(dotpos=0;
	    name[dotpos] && dotpos < limit && name[dotpos] != dot ;
	    dotpos++) {
		if(name[dotpos] == tilda) {
			tildapos = dotpos;
			seqnum = 0;
			maxseq = 1;
		} else if (name[dotpos] >= '0' && name[dotpos] <= '9') {
			seqnum = seqnum * 10 + name[dotpos] - '0';
			maxseq = maxseq * 10;
		} else
			tildapos = -1; /* sequence number interrupted */
	}
	if(tildapos == -1) {
		/* no sequence number yet */
		if(dotpos > limit - 2) {
			tildapos = limit - 2;
			dotpos = limit;
		} else {
			tildapos = dotpos;
			dotpos += 2;
		}
		seqnum = 1;
	} else {
		if(bump)
			seqnum++;
		if(seqnum > 999999) {
			seqnum = 1;
			tildapos = dotpos - 2;
			/* this matches Win95's behavior, and also guarantees
			 * us that the sequence numbers never get shorter */
		}
		if (seqnum == maxseq) {
		    if(dotpos >= limit)
			tildapos--;
		    else
			dotpos++;
		}
	}

	tmp = name[dotpos];
	if((bump && seqnum == 1) || seqnum > 1 || mtools_numeric_tail)
		sprintf(name+tildapos,"%c%d",tilda, seqnum);
	if(dot)
	    name[dotpos]=tmp;
	/* replace the character if it wasn't a space */
}


void autorename_short(char *name, int bump)
{
	autorename(name, '~', ' ', short_illegals, 8, bump);
}

void autorename_long(char *name, int bump)
{
	autorename(name, '-', '\0', long_illegals, 255, bump);
}


static inline int unicode_read(struct unicode_char *in, char *out, int num)
{
	char *end_out = out+num;

	while(out < end_out) {
		if (in->uchar)
			*out = '_';
		else
			*out = in->lchar;
		++out;
		++in;
	}
	return num;
}


void clear_vfat(struct vfat_state *v)
{
	v->subentries = 0;
	v->status = 0;
	v->present = 0;
}


/* sum_shortname
 *
 * Calculate the checksum that results from the short name in *dir.
 *
 * The sum is formed by circularly right-shifting the previous sum
 * and adding in each character, from left to right, padding both
 * the name and extension to maximum length with spaces and skipping
 * the "." (hence always summing exactly 11 characters).
 * 
 * This exact algorithm is required in order to remain compatible
 * with Microsoft Windows-95 and Microsoft Windows NT 3.5.
 * Thanks to Jeffrey Richter of Microsoft Systems Journal for
 * pointing me to the correct algorithm.
 *
 * David C. Niemi (niemi@tux.org) 95.01.19
 */
static inline unsigned char sum_shortname(char *name)
{
	unsigned char sum;
	char *end = name+11;

	for (sum=0; name<end; ++name)
		sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) 
		  + (*name ? *name : ' ');
	return(sum);
}

/* check_vfat
 *
 * Inspect a directory and any associated VSEs.
 * Return 1 if the VSEs comprise a valid long file name,
 * 0 if not.
 */
static inline void check_vfat(struct vfat_state *v, struct directory *dir)
{
	char name[12];

	if (! v->subentries) {
#ifdef DEBUG
		fprintf(stderr, "check_vfat: no VSEs.\n");
#endif
		return;
	}

	strncpy((char *)name, (char *)dir->name, 8);
	strncpy((char *)name + 8, (char *)dir->ext, 3);
	name[11] = '\0';

	if (v->sum != sum_shortname(name))
		return;
	
	if( (v->status & ((1<<v->subentries) - 1)) != (1<<v->subentries) - 1)
		return; /* missing entries */

	/* zero out byte following last entry, for good measure */
	v->name[VSE_NAMELEN * v->subentries] = 0;
	v->present = 1;
}


int clear_vses(Stream_t *Dir, int entrySlot, size_t last)
{
	direntry_t entry;
	dirCache_t *cache;
	int error;

	entry.Dir = Dir;
	entry.entry = entrySlot;

	/*maximize(last, entry.entry + MAX_VFAT_SUBENTRIES);*/
	cache = allocDirCache(Dir, last);
	if(!cache) {
		fprintf(stderr, "Out of memory error in clear_vses\n");
		exit(1);
	}
	addFreeEntry(cache, entry.entry, last);
	for (; entry.entry < last; ++entry.entry) {
#ifdef DEBUG
		fprintf(stderr,"Clearing entry %d.\n", entry.entry);
#endif
		dir_read(&entry, &error);
		if(error)
		    return error;
		if(!entry.dir.name[0] || entry.dir.name[0] == DELMARK)
			break;
		entry.dir.name[0] = DELMARK;
		if (entry.dir.attr == 0xf)
			entry.dir.attr = '\0';
		low_level_dir_write(&entry);
	}
	return 0;
}

int write_vfat(Stream_t *Dir, char *shortname, char *longname, int start,
	       direntry_t *mainEntry)
{
	struct vfat_subentry *vse;
	int vse_id, num_vses;
	char *c;
	direntry_t entry;
	dirCache_t *cache;
	char unixyName[13];
	
	if(longname) {
#ifdef DEBUG
		printf("Entering write_vfat with longname=\"%s\", start=%d.\n",
		       longname,start);
#endif
		entry.Dir = Dir;
		vse = (struct vfat_subentry *) &entry.dir;
		/* Fill in invariant part of vse */
		vse->attribute = 0x0f;
		vse->hash1 = vse->sector_l = vse->sector_u = 0;
		vse->sum = sum_shortname(shortname);
#ifdef DEBUG
		printf("Wrote checksum=%d for shortname %s.\n", 
		       vse->sum,shortname);
#endif
		num_vses = strlen(longname)/VSE_NAMELEN + 1;
		for (vse_id = num_vses; vse_id; --vse_id) {
			int end = 0;
			
			c = longname + (vse_id - 1) * VSE_NAMELEN;
			
			c += unicode_write(c, vse->text1, VSE1SIZE, &end);
			c += unicode_write(c, vse->text2, VSE2SIZE, &end);
			c += unicode_write(c, vse->text3, VSE3SIZE, &end);

			vse->id = (vse_id == num_vses) ? (vse_id | VSE_LAST) : vse_id;
#ifdef DEBUG
			printf("Writing longname=(%s), VSE %d (%13s) at %d, end = %d.\n",
			       longname, vse_id, longname + (vse_id-1) * VSE_NAMELEN,
			       start + num_vses - vse_id, start + num_vses);
#endif
			
			entry.entry = start + num_vses - vse_id;
			low_level_dir_write(&entry);
		}
	} else
		num_vses = 0;
	cache = allocDirCache(Dir, start + num_vses + 1);
	if(!cache) {
		fprintf(stderr, "Out of memory error\n");
		exit(1);
	}
	unix_name(shortname, shortname+8, 0, unixyName);
	addUsedEntry(cache, start, start + num_vses + 1, longname, unixyName,
		     &mainEntry->dir);
	low_level_dir_write(mainEntry);
	return start + num_vses;
}

void dir_write(direntry_t *entry)
{
	dirCacheEntry_t *dce;
	dirCache_t *cache;

	if(entry->entry == -3) {
		fprintf(stderr, "Attempt to write root directory pointer\n");
		exit(1);
	}

	cache = allocDirCache(entry->Dir, entry->entry + 1);
	if(!cache) {
		fprintf(stderr, "Out of memory error in dir_write\n");
		exit(1);
	}
	dce = cache->entries[entry->entry];
	if(dce) {
		if(entry->dir.name[0] == DELMARK) {
			addFreeEntry(cache, dce->beginSlot, dce->endSlot);
		} else {
			dce->dir = entry->dir;
		}
	}
	low_level_dir_write(entry);
}


/* 
 * The following function translates a series of vfat_subentries into
 * data suitable for a dircache entry
 */
static inline void parse_vses(direntry_t *entry,			      
			      struct vfat_state *v)
{
	struct vfat_subentry *vse;
	unsigned char id, last_flag;
	char *c;
	
	vse = (struct vfat_subentry *) &entry->dir;
	
	id = vse->id & VSE_MASK;
	last_flag = (vse->id & VSE_LAST);
	if (id > MAX_VFAT_SUBENTRIES) {
		fprintf(stderr, "parse_vses: invalid VSE ID %d at %d.\n",
			id, entry->entry);
		return;
	}
	
/* 950819: This code enforced finding the VSEs in order.  Well, Win95
 * likes to write them in *reverse* order for some bizarre reason!  So
 * we pretty much have to tolerate them coming in any possible order.
 * So skip this check, we'll do without it (What does this do, Alain?).
 *
 * 950820: Totally rearranged code to tolerate any order but to warn if
 * they are not in reverse order like Win95 uses.
 *
 * 950909: Tolerate any order. We recognize new chains by mismatching
 * checksums. In the event that the checksums match, new entries silently
 * overwrite old entries of the same id. This should accept all valid
 * entries, but may fail to reject invalid entries in some rare cases.
 */

	/* bad checksum, begin new chain */
	if(v->sum != vse->sum) {
		clear_vfat(v);
		v->sum = vse->sum;
	}
	
#ifdef DEBUG
	if(v->status & (1 << (id-1)))
		fprintf(stderr,
			"parse_vses: duplicate VSE %d\n", vse->id);
#endif
	
	v->status |= 1 << (id-1);
	if(last_flag)
		v->subentries = id;
	
#ifdef DEBUG
	if (id > v->subentries)
		/* simple test to detect entries preceding
		 * the "last" entry (really the first) */
		fprintf(stderr,
			"parse_vses: new VSE %d sans LAST flag\n",
			vse->id);
#endif

	c = &(v->name[VSE_NAMELEN * (id-1)]);
	c += unicode_read(vse->text1, c, VSE1SIZE);
	c += unicode_read(vse->text2, c, VSE2SIZE);
	c += unicode_read(vse->text3, c, VSE3SIZE);
#ifdef DEBUG
	printf("Read VSE %d at %d, subentries=%d, = (%13s).\n",
	       id,entry->entry,v->subentries,&(v->name[VSE_NAMELEN * (id-1)]));
#endif		
	if (last_flag)
		*c = '\0';	/* Null terminate long name */
}


static dirCacheEntry_t *vfat_lookup_loop_common(direntry_t *direntry,
						dirCache_t *cache,
						int lookForFreeSpace,
						int *io_error)
{
	char newfile[13];
	int initpos = direntry->entry + 1;
	struct vfat_state vfat;
	char *longname;
	int error;

	/* not yet cached */
	*io_error = 0;
	clear_vfat(&vfat);
	while(1) {
		++direntry->entry;
		if(!dir_read(direntry, &error)){
			if(error) {
			    *io_error = error;
			    return NULL;
			}
			addFreeEntry(cache, initpos, direntry->entry);
			return addEndEntry(cache, direntry->entry);
		}
		
		if (direntry->dir.name[0] == '\0'){
				/* the end of the directory */
			if(lookForFreeSpace)
				continue;
			return addEndEntry(cache, direntry->entry);
		}
		if(direntry->dir.name[0] != DELMARK &&
		   direntry->dir.attr == 0x0f)
			parse_vses(direntry, &vfat);
		else
			/* the main entry */
			break;
	}
	
	/* If we get here, it's a short name FAT entry, maybe erased.
	 * thus we should make sure that the vfat structure will be
	 * cleared before the next loop run */
	
	/* deleted file */
	if (direntry->dir.name[0] == DELMARK) {
		return addFreeEntry(cache, initpos, 
				    direntry->entry + 1);
	}
	
	check_vfat(&vfat, &direntry->dir);
	if(!vfat.present)
		vfat.subentries = 0;
	
	/* mark space between last entry and this one as free */
	addFreeEntry(cache, initpos, 
		     direntry->entry - vfat.subentries);
	
	if (direntry->dir.attr & 0x8){
		strncpy(newfile, direntry->dir.name,8);
		newfile[8]='\0';
		strncat(newfile, direntry->dir.ext,3);
		newfile[11]='\0';
	} else
		unix_name(direntry->dir.name, 
			  direntry->dir.ext, 
			  direntry->dir.Case, 
			  newfile);

	if(vfat.present)
		longname = vfat.name;
	else
		longname = 0;

	return addUsedEntry(cache, direntry->entry - vfat.subentries,
			    direntry->entry + 1, longname, 
			    newfile, &direntry->dir);
}

static inline dirCacheEntry_t *vfat_lookup_loop_for_read(direntry_t *direntry,
							 dirCache_t *cache,
							 int *io_error)
{
	int initpos = direntry->entry + 1;
	dirCacheEntry_t *dce;

	*io_error = 0;
	dce = cache->entries[initpos];
	if(dce) {
		direntry->entry = dce->endSlot - 1;
		return dce;
	} else {
		return vfat_lookup_loop_common(direntry, cache, 0, io_error);
	}
}


typedef enum result_t {
	RES_NOMATCH,
	RES_MATCH,
	RES_END,
	RES_ERROR
} result_t;


/* 
 * 0 does not match
 * 1 matches
 * 2 end
 */
static result_t checkNameForMatch(struct direntry_t *direntry, 
				  dirCacheEntry_t *dce,
				  const char *filename,
				  char *longname,
				  char *shortname,
				  int length,
				  int flags)
{
	switch(dce->type) {
		case DCET_FREE:
			return RES_NOMATCH;
		case DCET_END:
			return RES_END;
		case DCET_USED:
			break;
		default:
			fprintf(stderr, "Unexpected entry type %d\n",
				dce->type);
			return RES_ERROR;
	}

	direntry->dir = dce->dir;

	/* make sure the entry is of an accepted type */
	if((direntry->dir.attr & 0x8) && !(flags & ACCEPT_LABEL))
		return RES_NOMATCH;


	/*---------- multiple files ----------*/
	if(!((flags & MATCH_ANY) ||
	     (dce->longName && 
	      match(dce->longName, filename, direntry->name, 0, length)) ||
	     match(dce->shortName, filename, direntry->name, 1, length))) {

		return RES_NOMATCH;
	}

	/* entry of non-requested type, has to come after name
	 * checking because of clash handling */
	if(IS_DIR(direntry) && !(flags & ACCEPT_DIR)) {
		if(!(flags & (ACCEPT_LABEL|MATCH_ANY|NO_MSG)))
			fprintf(stderr,
				"Skipping \"%s\", is a directory\n",
				dce->shortName);
		return RES_NOMATCH;
	}

	if(!(direntry->dir.attr & (ATTR_LABEL | ATTR_DIR)) && 
	   !(flags & ACCEPT_PLAIN)) {
		if(!(flags & (ACCEPT_LABEL|MATCH_ANY|NO_MSG)))
			fprintf(stderr,
				"Skipping \"%s\", is not a directory\n",
				dce->shortName);
		return RES_NOMATCH;
	}

	return RES_MATCH;
}


/*
 * vfat_lookup looks for filenames in directory dir.
 * if a name if found, it is returned in outname
 * if applicable, the file is opened and its stream is returned in File
 */

int vfat_lookup(direntry_t *direntry, const char *filename, int length,
		int flags, char *shortname, char *longname)
{
	dirCacheEntry_t *dce;
	result_t result;
	dirCache_t *cache;
	int io_error;

	if(length == -1 && filename)
		length = strlen(filename);

	if (direntry->entry == -2)
		return -1;

	cache = allocDirCache(direntry->Dir, direntry->entry+1);
	if(!cache) {
		fprintf(stderr, "Out of memory error in vfat_lookup [0]\n");
		exit(1);
	}

	do {
		dce = vfat_lookup_loop_for_read(direntry, cache, &io_error);
		if(!dce) {
			if (io_error)
				return -2;
			fprintf(stderr, "Out of memory error in vfat_lookup\n");
			exit(1);
		}
		result = checkNameForMatch(direntry, dce,
					   filename, 
					   longname, shortname,
					   length, flags);
	} while(result == RES_NOMATCH);

	if(result == RES_MATCH){
		if(longname){
			if(dce->longName)
				strcpy(longname, dce->longName);
			else
				*longname ='\0';
		}
		if(shortname)
			strcpy(shortname, dce->shortName);
		direntry->beginSlot = dce->beginSlot;
		direntry->endSlot = dce->endSlot-1;
		return 0; /* file found */
	} else {
		direntry->entry = -2;
		return -1; /* no file found */
	}
}

static inline dirCacheEntry_t *vfat_lookup_loop_for_insert(direntry_t *direntry,
							   int initpos,
							   dirCache_t *cache)
{
	dirCacheEntry_t *dce;
	int io_error;

	dce = cache->entries[initpos];
	if(dce && dce->type != DCET_END) {
		return dce;
	} else {
		direntry->entry = initpos - 1;
		dce = vfat_lookup_loop_common(direntry, cache, 1, &io_error);
		if(!dce) {
			if (io_error) {
				return NULL;
			}
			fprintf(stderr, 
				"Out of memory error in vfat_lookup_loop\n");
			exit(1);
		}
		return cache->entries[initpos];
	}
}

static void accountFreeSlots(struct scan_state *ssp, dirCacheEntry_t *dce)
{
	if(ssp->got_slots)
		return;

	if(ssp->free_end != dce->beginSlot) {
		ssp->free_start = dce->beginSlot;
	}
	ssp->free_end = dce->endSlot;

	if(ssp->free_end - ssp->free_start >= ssp->size_needed) {
		ssp->got_slots = 1;
		ssp->slot = ssp->free_start + ssp->size_needed - 1;
	}
}

/* lookup_for_insert replaces the old scandir function.  It directly
 * calls into vfat_lookup_loop, thus eliminating the overhead of the
 * normal vfat_lookup
 */
int lookupForInsert(Stream_t *Dir,
					char *dosname,
					char *longname,
					struct scan_state *ssp, 
					int ignore_entry,
					int source_entry,
					int pessimisticShortRename)
{
	direntry_t entry;
	int ignore_match;
	dirCacheEntry_t *dce;
	dirCache_t *cache;
	int pos; /* position _before_ the next answered entry */
	char shortName[13];

	ignore_match = (ignore_entry == -2 );

	initializeDirentry(&entry, Dir);
	ssp->match_free = 0;

	/* hash bitmap of already encountered names.  Speeds up batch appends
	 * to huge directories, because in the best case, we only need to scan
	 * the new entries rather than the whole directory */
	cache = allocDirCache(Dir, 1);
	if(!cache) {
		fprintf(stderr, "Out of memory error in lookupForInsert\n");
		exit(1);
	}

	if(!ignore_match)
		unix_name(dosname, dosname + 8, 0, shortName);

	pos = cache->nrHashed;
	if(source_entry >= 0 ||
	   (pos && isHashed(cache, longname))) {
		pos = 0;
	} else if(pos && !ignore_match && isHashed(cache, shortName)) {
		if(pessimisticShortRename) {
			ssp->shortmatch = -2;
			return 1;
		}
		pos = 0;
	} else if(growDirCache(cache, pos) < 0) {
		fprintf(stderr, "Out of memory error in vfat_looup [0]\n");
		exit(1);
	}
	do {
		dce = vfat_lookup_loop_for_insert(&entry, pos, cache);
		switch(dce->type) {
			case DCET_FREE:
				accountFreeSlots(ssp, dce);
				break;
			case DCET_USED:
				if(!(dce->dir.attr & 0x8) &&
				   dce->endSlot - 1 == source_entry)
				   accountFreeSlots(ssp, dce);

				/* labels never match, neither does the 
				 * ignored entry */
				if( (dce->dir.attr & 0x8) ||
				    (dce->endSlot - 1 == ignore_entry) )
					break;

				/* check long name */
				if((dce->longName && 
				    !strcasecmp(dce->longName, longname)) ||
				   (dce->shortName &&
				    !strcasecmp(dce->shortName, longname))) {
					ssp->longmatch = dce->endSlot - 1;
					/* long match is a reason for
					 * immediate stop */
					return 1;
				}

				/* Long name or not, always check for 
				 * short name match */
				if (!ignore_match &&
				    !strcasecmp(shortName, dce->shortName))
					ssp->shortmatch = dce->endSlot - 1;
				break;
			case DCET_END:
				break;
		}
		pos = dce->endSlot;
	} while(dce->type != DCET_END);
	if (ssp->shortmatch > -1)
		return 1;
	ssp->max_entry = dce->beginSlot;
	if (ssp->got_slots)
		return 6;	/* Success */

	/* Need more room.  Can we grow the directory? */
	if(!isRootDir(Dir))		
		return 5;	/* OK, try to grow the directory */

	fprintf(stderr, "No directory slots\n");
	return -1;
}



/* End vfat.c */

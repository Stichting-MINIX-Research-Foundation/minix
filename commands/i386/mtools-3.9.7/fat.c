#include "sysincludes.h"
#include "msdos.h"
#include "stream.h"
#include "mtools.h"
#include "fsP.h"

extern Stream_t *default_drive;

#ifdef HAVE_LONG_LONG
typedef long long fatBitMask;
#else
typedef long fatBitMask;
#endif

typedef struct FatMap_t {
	unsigned char *data;
	fatBitMask dirty;
	fatBitMask valid;
} FatMap_t;

#define SECT_PER_ENTRY (sizeof(fatBitMask)*8)
#define ONE ((fatBitMask) 1)

static inline int readSector(Fs_t *This, char *buf, unsigned int off,
					  size_t size)
{
	return READS(This->Next, buf, sectorsToBytes((Stream_t *)This, off), 
				 size << This->sectorShift);
}


static inline int forceReadSector(Fs_t *This, char *buf, unsigned int off,
								  size_t size)
{
	return force_read(This->Next, buf, sectorsToBytes((Stream_t *)This, off), 
					  size << This->sectorShift);
}


static inline int writeSector(Fs_t *This, char *buf, unsigned int off,
							  size_t size)
{
	return WRITES(This->Next, buf, sectorsToBytes((Stream_t*)This, off), 
				  size << This->sectorShift);
}

static inline int forceWriteSector(Fs_t *This, char *buf, unsigned int off,
					  size_t size)
{
	return force_write(This->Next, buf, sectorsToBytes((Stream_t*)This, off), 
					   size << This->sectorShift);
}


static FatMap_t *GetFatMap(Fs_t *Stream)
{
	int nr_entries,i;
	FatMap_t *map;

	Stream->fat_error = 0;
	nr_entries = (Stream->fat_len + SECT_PER_ENTRY - 1) / SECT_PER_ENTRY;
	map = NewArray(nr_entries, FatMap_t);
	if(!map)
		return 0;

	for(i=0; i< nr_entries; i++) {
		map[i].data = 0;
		map[i].valid = 0;
		map[i].dirty = 0;
	}

	return map;
}

static inline int locate(Fs_t *Stream, int offset, int *slot, int *bit)
{
	if(offset >= Stream->fat_len)
		return -1;
	*slot = offset / SECT_PER_ENTRY;
	*bit = offset % SECT_PER_ENTRY;
	return 0;
}

static inline int fatReadSector(Fs_t *This, int sector, int slot, 
				int bit, int dupe)
{
	int fat_start, ret;

	dupe = (dupe + This->primaryFat) % This->num_fat;
	fat_start = This->fat_start + This->fat_len * dupe;
	
	/* first, read as much as the buffer can give us */
	ret = readSector(This,
					 (char *)(This->FatMap[slot].data+(bit<<This->sectorShift)),
					 fat_start+sector,
					 (SECT_PER_ENTRY - bit%SECT_PER_ENTRY));
	if(ret < 0)
		return 0;

	if(ret < This->sector_size) {
		/* if we got less than one sector's worth, insist to get at
		 * least one sector */
		ret = forceReadSector(This,
							  (char *) (This->FatMap[slot].data + 
										(bit << This->sectorShift)),
							  fat_start+sector, 1);
		if(ret < This->sector_size)
			return 0;
		return 1;
	}

	return ret >> This->sectorShift;
}


static int fatWriteSector(Fs_t *This, int sector, int slot, int bit, int dupe)
{
	int fat_start;

	dupe = (dupe + This->primaryFat) % This->num_fat;
	if(dupe && !This->writeAllFats)
		return This->sector_size;

	fat_start = This->fat_start + This->fat_len * dupe;

	return forceWriteSector(This,
							(char *) 
							(This->FatMap[slot].data + bit * This->sector_size),
							fat_start+sector, 1);
}

static unsigned char *loadSector(Fs_t *This,
				 unsigned int sector, fatAccessMode_t mode,
				 int recurs)
{
	int slot, bit, i, ret;

	if(locate(This,sector, &slot, &bit) < 0)
		return 0;
#if 0
        if (((This->fat_len + SECT_PER_ENTRY - 1) / SECT_PER_ENTRY) <= slot) {
		fprintf(stderr,"This should not happen\n");
		fprintf(stderr, "fat_len = %d\n", This->fat_len);
		fprintf(stderr, "SECT_PER_ENTRY=%d\n", (int)SECT_PER_ENTRY);
		fprintf(stderr, "sector = %d slot = %d bit=%d\n", 
			sector, slot, bit);
		fprintf(stderr, "left = %d",(int)
			((This->fat_len+SECT_PER_ENTRY-1) / SECT_PER_ENTRY));
                return 0;
	}
#endif
	if(!This->FatMap[slot].data) {
		/* allocate the storage space */
		This->FatMap[slot].data = 
			malloc(This->sector_size * SECT_PER_ENTRY);
		if(!This->FatMap[slot].data)
			return 0;
		memset(This->FatMap[slot].data, 0xee,
		       This->sector_size * SECT_PER_ENTRY);
	}

	if(! (This->FatMap[slot].valid & (ONE << bit))) {
		ret = -1;
		for(i=0; i< This->num_fat; i++) {
			/* read the sector */
			ret = fatReadSector(This, sector, slot, bit, i);

			if(ret == 0) {
				fprintf(stderr,
					"Error reading fat number %d\n", i);
				continue;
			}
			break;
		}

		/* all copies bad.  Return error */
		if(ret == 0)
			return 0;

		for(i=0; i < ret; i++)
			This->FatMap[slot].valid |= ONE << (bit + i);

		if(!recurs && ret == 1)
			/* do some prefetching, if we happened to only
			 * get one sector */
			loadSector(This, sector+1, mode, 1);
		if(!recurs && batchmode)
			for(i=0; i < 1024; i++)
				loadSector(This, sector+i, mode, 1);
	}

	if(mode == FAT_ACCESS_WRITE) {
		This->FatMap[slot].dirty |= ONE << bit;
		This->fat_dirty = 1;
	}
	return This->FatMap[slot].data + (bit << This->sectorShift);
}


static unsigned char *getAddress(Fs_t *Stream,
				 unsigned int num, fatAccessMode_t mode)
{
	unsigned char *ret;
	int sector;
	int offset;

	sector = num >> Stream->sectorShift;
	ret = 0;
	if(sector == Stream->lastFatSectorNr &&
	   Stream->lastFatAccessMode >= mode)
		ret = Stream->lastFatSectorData;
	if(!ret) {		
		ret = loadSector(Stream, sector, mode, 0);
		if(!ret)
			return 0;
		Stream->lastFatSectorNr = sector;
		Stream->lastFatSectorData = ret;
		Stream->lastFatAccessMode = mode;
	}
	offset = num & Stream->sectorMask;
	return ret+offset;
}


static int readByte(Fs_t *Stream, int start)
{
	unsigned char *address;
	
	address = getAddress(Stream, start, FAT_ACCESS_READ);
	if(!address)
		return -1;
	return *address;
}


/*
 * Fat 12 encoding:
 *	|    byte n     |   byte n+1    |   byte n+2    |
 *	|7|6|5|4|3|2|1|0|7|6|5|4|3|2|1|0|7|6|5|4|3|2|1|0|
 *	| | | | | | | | | | | | | | | | | | | | | | | | |
 *	| n+0.0 | n+0.5 | n+1.0 | n+1.5 | n+2.0 | n+2.5 |
 *	    \_____  \____   \______/________/_____   /
 *	      ____\______\________/   _____/  ____\_/
 *	     /     \      \          /       /     \
 *	| n+1.5 | n+0.0 | n+0.5 | n+2.0 | n+2.5 | n+1.0 |
 *	|      FAT entry k      |    FAT entry k+1      |
 */
 
 /*
 * Get and decode a FAT (file allocation table) entry.  Returns the cluster
 * number on success or 1 on failure.
 */

static unsigned int fat12_decode(Fs_t *Stream, unsigned int num)
{
	unsigned int start = num * 3 / 2;
	int byte0 = readByte(Stream, start);
	int byte1 = readByte(Stream, start+1);
       
	if (num < 2 || byte0 < 0 || byte1 < 0 || num > Stream->num_clus+1) {
		fprintf(stderr,"[1] Bad address %d\n", num);
		return 1;
	}

	if (num & 1)
		return (byte1 << 4) | ((byte0 & 0xf0)>>4);
	else
		return ((byte1 & 0xf) << 8) | byte0;
}


/*
 * Puts a code into the FAT table.  Is the opposite of fat_decode().  No
 * sanity checking is done on the code.  Returns a 1 on error.
 */
static void fat12_encode(Fs_t *Stream, unsigned int num, unsigned int code)
{
	int start = num * 3 / 2;
	unsigned char *address0 = getAddress(Stream, start, FAT_ACCESS_WRITE);
	unsigned char *address1 = getAddress(Stream, start+1, FAT_ACCESS_WRITE);

	if (num & 1) {
		/* (odd) not on byte boundary */
		*address0 = (*address0 & 0x0f) | ((code << 4) & 0xf0);
		*address1 = (code >> 4) & 0xff;
	} else {
		/* (even) on byte boundary */
		*address0 = code & 0xff;
		*address1 = (*address1 & 0xf0) | ((code >> 8) & 0x0f);
	}
}


/*
 * Fat 16 encoding:
 *	|    byte n     |   byte n+1    |
 *	|7|6|5|4|3|2|1|0|7|6|5|4|3|2|1|0|
 *	| | | | | | | | | | | | | | | | |
 *	|         FAT entry k           |
 */

static unsigned int fat16_decode(Fs_t *Stream, unsigned int num)
{
	unsigned char *address = getAddress(Stream, num << 1, FAT_ACCESS_READ);
	return _WORD(address);
}

static void fat16_encode(Fs_t *Stream, unsigned int num, unsigned int code)
{       
	unsigned char *address = getAddress(Stream, num << 1, FAT_ACCESS_WRITE);
	set_word(address, code);
}


static unsigned int fast_fat16_decode(Fs_t *Stream, unsigned int num)
{
	unsigned short *address = 
		(unsigned short *) getAddress(Stream, num << 1, 
					      FAT_ACCESS_READ);
	return *address;
}

static void fast_fat16_encode(Fs_t *Stream, unsigned int num, unsigned int code)
{       
	unsigned short *address = 
		(unsigned short *) getAddress(Stream, num << 1, 
					      FAT_ACCESS_WRITE);
	*address = code;
}




/*
 * Fat 32 encoding
 */
static unsigned int fat32_decode(Fs_t *Stream, unsigned int num)
{
	unsigned char *address = getAddress(Stream, num << 2, FAT_ACCESS_READ);
	return _DWORD(address);
}

static void fat32_encode(Fs_t *Stream, unsigned int num, unsigned int code)
{       
	unsigned char *address = getAddress(Stream, num << 2, FAT_ACCESS_WRITE);
	set_dword(address, code);
}


static unsigned int fast_fat32_decode(Fs_t *Stream, unsigned int num)
{
	unsigned int *address = 
		(unsigned int *) getAddress(Stream, num << 2, 
					    FAT_ACCESS_READ);
	return *address;
}

static void fast_fat32_encode(Fs_t *Stream, unsigned int num, unsigned int code)
{       
	unsigned int *address = 
		(unsigned int *) getAddress(Stream, num << 2, 
					    FAT_ACCESS_WRITE);
	*address = code;
}


/*
 * Write the FAT table to the disk.  Up to now the FAT manipulation has
 * been done in memory.  All errors are fatal.  (Might not be too smart
 * to wait till the end of the program to write the table.  Oh well...)
 */

void fat_write(Fs_t *This)
{
	int i, j, dups, ret, bit, slot;
	int fat_start;

	/*fprintf(stderr, "Fat write\n");*/

	if (!This->fat_dirty)
		return;

	dups = This->num_fat;
	if (This->fat_error)
		dups = 1;


	for(i=0; i<dups; i++){
		j = 0;
		fat_start = This->fat_start + i*This->fat_len;
		for(slot=0;j<This->fat_len;slot++) {
			if(!This->FatMap[slot].dirty) {
				j += SECT_PER_ENTRY;
				continue;
			}
			for(bit=0; 
			    bit < SECT_PER_ENTRY && j<This->fat_len;
			    bit++,j++) {
				if(!(This->FatMap[slot].dirty & (ONE << bit)))
					continue;
				ret = fatWriteSector(This,j,slot, bit, i);
				if (ret < This->sector_size){
					if (ret < 0 ){
						perror("error in fat_write");
						exit(1);
					} else {
						fprintf(stderr,
							"end of file in fat_write\n");
						exit(1);
					}
				}
				/* if last dupe, zero it out */
				if(i==dups-1)
					This->FatMap[slot].dirty &= ~(1<<bit);
			}
		}	 
	}
	/* write the info sector, if any */
	if(This->infoSectorLoc && This->infoSectorLoc != MAX32) {
		/* initialize info sector */
		InfoSector_t *infoSector;
		infoSector = (InfoSector_t *) safe_malloc(This->sector_size);
		set_dword(infoSector->signature1, INFOSECT_SIGNATURE1);
		memset(infoSector->filler1, sizeof(infoSector->filler1),0);
		memset(infoSector->filler2, sizeof(infoSector->filler2),0);
		set_dword(infoSector->signature2, INFOSECT_SIGNATURE2);
		set_dword(infoSector->pos, This->last);
		set_dword(infoSector->count, This->freeSpace);
		set_dword(infoSector->signature3, 0xaa55);
		if(forceWriteSector(This, (char *)infoSector, This->infoSectorLoc, 1) !=
		   This->sector_size)
			fprintf(stderr,"Trouble writing the info sector\n");
		free(infoSector);
	}
	This->fat_dirty = 0;
	This->lastFatAccessMode = FAT_ACCESS_READ;
}



/*
 * Zero-Fat
 * Used by mformat.
 */
int zero_fat(Fs_t *Stream, int media_descriptor)
{
	int i, j;
	int fat_start;
	unsigned char *buf;

	buf = malloc(Stream->sector_size);
	if(!buf) {
		perror("alloc fat sector buffer");
		return -1;
	}
	for(i=0; i< Stream->num_fat; i++) {
		fat_start = Stream->fat_start + i*Stream->fat_len;
		for(j = 0; j < Stream->fat_len; j++) {
			if(j <= 1)
				memset(buf, 0, Stream->sector_size);
			if(!j) {
				buf[0] = media_descriptor;
				buf[2] = buf[1] = 0xff;
				if(Stream->fat_bits > 12)
					buf[3] = 0xff;
				if(Stream->fat_bits > 16) {
					buf[4] = 0xff;
					buf[5] = 0xff;
					buf[6] = 0xff;
					buf[7] = 0x0f;
				}
			}

			if(forceWriteSector(Stream, (char *)buf,
								fat_start + j, 1) !=
			   Stream->sector_size) {
				fprintf(stderr,
						"Trouble initializing a FAT sector\n");
				free(buf);
				return -1;
			}
		}
	}
	
	free(buf);
	Stream->FatMap = GetFatMap(Stream);
	if (Stream->FatMap == NULL) {
		perror("alloc fat map");
		return -1;
	}
	return 0;
}


void set_fat12(Fs_t *This)
{
	This->fat_bits = 12;
	This->end_fat = 0xfff;
	This->last_fat = 0xff6;
	This->fat_decode = fat12_decode;
	This->fat_encode = fat12_encode;
}

static char word_endian_test[] = { 0x34, 0x12 };

void set_fat16(Fs_t *This)
{
	This->fat_bits = 16;
	This->end_fat = 0xffff;
	This->last_fat = 0xfff6;

	if(sizeof(unsigned short) == 2 &&  
	   * (unsigned short *) word_endian_test == 0x1234) {
		This->fat_decode = fast_fat16_decode;
		This->fat_encode = fast_fat16_encode;
	} else {
		This->fat_decode = fat16_decode;
		This->fat_encode = fat16_encode;
	}
}

static char dword_endian_test[] = { 0x78, 0x56, 0x34, 0x12 };

void set_fat32(Fs_t *This)
{
	This->fat_bits = 32;
	This->end_fat = 0xfffffff;
	This->last_fat = 0xffffff6;
	
	if(sizeof(unsigned int) == 4 &&  
	   * (unsigned int *) dword_endian_test == 0x12345678) {
		This->fat_decode = fast_fat32_decode;
		This->fat_encode = fast_fat32_encode;
	} else {
		This->fat_decode = fat32_decode;
		This->fat_encode = fat32_encode;
	}
}


static int check_fat(Fs_t *This)
{
	/* 
	 * This is only a sanity check.  For disks with really big FATs,
	 * there is no point in checking the whole FAT.
	 */

	int i, f, tocheck;
	if(mtools_skip_check)
		return 0;

	/* too few sectors in the FAT */
	if(This->fat_len < NEEDED_FAT_SIZE(This))
		return -1;
	/* we do not warn about too much sectors in FAT, which may
	 * happen when a partition has been shrunk using FIPS, or on
	 * other occurrences */
	
	tocheck = This->num_clus;
	if (tocheck < 0 || tocheck + 1 >= This->last_fat) {
		fprintf(stderr, "Too many clusters in FAT\n");
		return -1;
	}

	if(tocheck > 4096)
		tocheck = 4096;

	for ( i= 3 ; i < tocheck; i++){
		f = This->fat_decode(This,i);
		if (f == 1 || (f < This->last_fat && f > This->num_clus)){
			fprintf(stderr,
				"Cluster # at %d too big(%#x)\n", i,f);
			fprintf(stderr,"Probably non MS-DOS disk\n");
			return -1;
		}
	}
	return 0;
}


/*
 * Read the first sector of FAT table into memory.  Crude error detection on
 * wrong FAT encoding scheme.
 */
static int check_media_type(Fs_t *This, struct bootsector *boot, 
			    unsigned int tot_sectors)
{
	unsigned char *address;

	This->num_clus = (tot_sectors - This->clus_start) / This->cluster_size;

	This->FatMap = GetFatMap(This);
	if (This->FatMap == NULL) {
		perror("alloc fat map");
		return -1;
	}

	address = getAddress(This, 0, FAT_ACCESS_READ);
	if(!address) {
		fprintf(stderr,
			"Could not read first FAT sector\n");
		return -1;
	}

	if(mtools_skip_check)
		return 0;

	if(!address[0] && !address[1] && !address[2])
		/* Some Atari disks have zeroes where Dos has media descriptor
		 * and 0xff.  Do not consider this as an error */
		return 0;
	
	if((address[0] != boot->descr && boot->descr >= 0xf0 &&
	    ((address[0] != 0xf9 && address[0] != 0xf7) 
	     || boot->descr != 0xf0)) || address[0] < 0xf0) {
		fprintf(stderr,
			"Bad media types %02x/%02x, probably non-MSDOS disk\n", 
				address[0],
				boot->descr);
		return -1;
	}

	if(address[1] != 0xff || address[2] != 0xff){
		fprintf(stderr,"Initial byte of fat is not 0xff\n");
		return -1;
	}

	return 0;
}

static int fat_32_read(Fs_t *This, struct bootsector *boot, 
		       unsigned int tot_sectors)
{
	int size;

	This->fat_len = DWORD(ext.fat32.bigFat);
	This->writeAllFats = !(boot->ext.fat32.extFlags[0] & 0x80);
	This->primaryFat = boot->ext.fat32.extFlags[0] & 0xf;
	This->rootCluster = DWORD(ext.fat32.rootCluster);
	This->clus_start = This->fat_start + This->num_fat * This->fat_len;

	/* read the info sector */
	size = This->sector_size;
	This->infoSectorLoc = WORD(ext.fat32.infoSector);
	if(This->sector_size >= 512 &&
	   This->infoSectorLoc && This->infoSectorLoc != MAX32) {
		InfoSector_t *infoSector;
		infoSector = (InfoSector_t *) safe_malloc(size);
		if(forceReadSector(This, (char *)infoSector,
						   This->infoSectorLoc, 1) == This->sector_size &&
		   _DWORD(infoSector->signature1) == INFOSECT_SIGNATURE1 &&
		   _DWORD(infoSector->signature2) == INFOSECT_SIGNATURE2) {
			This->freeSpace = _DWORD(infoSector->count);
			This->last = _DWORD(infoSector->pos);
		}
		free(infoSector);
	}
	
	set_fat32(This);
	return(check_media_type(This,boot, tot_sectors) ||
	       check_fat(This));
}


static int old_fat_read(Fs_t *This, struct bootsector *boot, 
						int config_fat_bits,
						size_t tot_sectors, int nodups)
{
	This->writeAllFats = 1;
	This->primaryFat = 0;
	This->dir_start = This->fat_start + This->num_fat * This->fat_len;
	This->clus_start = This->dir_start + This->dir_len;
	This->infoSectorLoc = MAX32;

	if(nodups)
		This->num_fat = 1;

	if(check_media_type(This,boot, tot_sectors))
		return -1;

	if(This->num_clus > FAT12) {
		set_fat16(This);
		/* third FAT byte must be 0xff */
		if(!mtools_skip_check && readByte(This, 3) != 0xff)
			return -1;
	} else
		set_fat12(This);

	return check_fat(This);
}

/*
 * Read the first sector of the  FAT table into memory and initialize 
 * structures.
 */
int fat_read(Fs_t *This, struct bootsector *boot, int fat_bits,
	   size_t tot_sectors, int nodups)
{
	This->fat_error = 0;
	This->fat_dirty = 0;
	This->last = MAX32;
	This->freeSpace = MAX32;
	This->lastFatSectorNr = 0;
	This->lastFatSectorData = 0;

	if(This->fat_len)
		return old_fat_read(This, boot, fat_bits, tot_sectors, nodups);
	else
		return fat_32_read(This, boot, tot_sectors);
}


unsigned int fatDecode(Fs_t *This, unsigned int pos)
{
	int ret;

	ret = This->fat_decode(This, pos);
	if(ret && (ret < 2 || ret > This->num_clus+1) && ret < This->last_fat) {
		fprintf(stderr, "Bad FAT entry %d at %d\n", ret, pos);
		This->fat_error++;
	}
	return ret;
}

/* append a new cluster */
void fatAppend(Fs_t *This, unsigned int pos, unsigned int newpos)
{
	This->fat_encode(This, pos, newpos);
	This->fat_encode(This, newpos, This->end_fat);
	if(This->freeSpace != MAX32)
		This->freeSpace--;
}

/* de-allocates the given cluster */
void fatDeallocate(Fs_t *This, unsigned int pos)
{
	This->fat_encode(This, pos, 0);
	if(This->freeSpace != MAX32)
		This->freeSpace++;
}

/* allocate a new cluster */
void fatAllocate(Fs_t *This, unsigned int pos, unsigned int value)
{
	This->fat_encode(This, pos, value);
	if(This->freeSpace != MAX32)
		This->freeSpace--;
}

void fatEncode(Fs_t *This, unsigned int pos, unsigned int value)
{
	unsigned int oldvalue = This->fat_decode(This, pos);
	This->fat_encode(This, pos, value);
	if(This->freeSpace != MAX32) {
		if(oldvalue)
			This->freeSpace++;
		if(value)
			This->freeSpace--;
	}
}

unsigned int get_next_free_cluster(Fs_t *This, unsigned int last)
{
	int i;

	if(This->last != MAX32)
		last = This->last;

	if (last < 2 ||
	    last >= This->num_clus+1)
		last = 1;

	for (i=last+1; i< This->num_clus+2; i++) {
		if (!fatDecode(This, i)) {
			This->last = i;
			return i;
		}
	}

	for(i=2; i < last+1; i++) {
		if (!fatDecode(This, i)) {
			This->last = i;
			return i;
		}
	}


	fprintf(stderr,"No free cluster %d %d\n", This->preallocatedClusters,
		This->last);
	return 1;
}

int fat_error(Stream_t *Dir)
{
	Stream_t *Stream = GetFs(Dir);
	DeclareThis(Fs_t);

	if(This->fat_error)
		fprintf(stderr,"Fat error detected\n");

	return This->fat_error;
}

int fat32RootCluster(Stream_t *Dir)
{
	Stream_t *Stream = GetFs(Dir);
	DeclareThis(Fs_t);
	
	if(This->fat_bits == 32)
		return This->rootCluster;
	else
		return 0;
}


/*
 * Get the amount of free space on the diskette
 */

mt_size_t getfree(Stream_t *Dir)
{
	Stream_t *Stream = GetFs(Dir);
	DeclareThis(Fs_t);

	if(This->freeSpace == MAX32 || This->freeSpace == 0) {
		register unsigned int i;
		size_t total;

		total = 0L;
		for (i = 2; i < This->num_clus + 2; i++)
			if (!fatDecode(This,i))
				total++;
		This->freeSpace = total;
	}
	return sectorsToBytes((Stream_t*)This, 
						  This->freeSpace * This->cluster_size);
}


/*
 * Ensure that there is a minimum of total sectors free
 */
int getfreeMinClusters(Stream_t *Dir, size_t size)
{
	Stream_t *Stream = GetFs(Dir);
	DeclareThis(Fs_t);
	register unsigned int i, last;
	size_t total;

	if(batchmode && This->freeSpace == MAX32)
		getfree(Stream);

	if(This->freeSpace != MAX32) {
		if(This->freeSpace >= size)
			return 1;
		else {
			fprintf(stderr, "Disk full\n");
			got_signal = 1;
			return 0;
		}
	}

	total = 0L;

	/* we start at the same place where we'll start later to actually
	 * allocate the sectors.  That way, the same sectors of the FAT, which
	 * are already loaded during getfreeMin will be able to be reused 
	 * during get_next_free_cluster */
	last = This->last;
	
	if ( last < 2 || last >= This->num_clus + 2)
		last = 1;
	for (i=last+1; i< This->num_clus+2; i++){
		if (!fatDecode(This, i))
			total++;
		if(total >= size)
			return 1;				
	}
	for(i=2; i < last+1; i++){
		if (!fatDecode(This, i))
			total++;
		if(total >= size)
			return 1;
	}
	fprintf(stderr, "Disk full\n");
	got_signal = 1;
	return 0;
}


int getfreeMinBytes(Stream_t *Dir, mt_size_t size)
{
	Stream_t *Stream = GetFs(Dir);
	DeclareThis(Fs_t);
	size_t size2;

	size2 = size  / (This->sector_size * This->cluster_size);
	if(size % (This->sector_size * This->cluster_size))
		size2++;
	return getfreeMinClusters(Dir, size2);
}


unsigned int getStart(Stream_t *Dir, struct directory *dir)
{
	Stream_t *Stream = GetFs(Dir);
	unsigned int first;

	first = START(dir);
	if(fat32RootCluster(Stream))
		first |= STARTHI(dir) << 16;
	return first;
}

int fs_free(Stream_t *Stream)
{
	DeclareThis(Fs_t);

	if(This->FatMap) {
		int i, nr_entries;
		nr_entries = (This->fat_len + SECT_PER_ENTRY - 1) / 
			SECT_PER_ENTRY;
		for(i=0; i< nr_entries; i++)
			if(This->FatMap[i].data)
				free(This->FatMap[i].data);		
		free(This->FatMap);
	}
	return 0;
}

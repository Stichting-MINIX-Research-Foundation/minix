#ifndef MTOOLS_FSP_H
#define MTOOLS_FSP_H

#include "stream.h"
#include "msdos.h"
#include "fs.h"

typedef enum fatAccessMode_t { 
	FAT_ACCESS_READ, 
	FAT_ACCESS_WRITE
} fatAccessMode_t;

typedef struct Fs_t {
	Class_t *Class;
	int refs;
	Stream_t *Next;
	Stream_t *Buffer;
	
	int serialized;
	unsigned long serial_number;
	int cluster_size;
	unsigned int sector_size;
	int fat_error;

	unsigned int (*fat_decode)(struct Fs_t *This, unsigned int num);
	void (*fat_encode)(struct Fs_t *This, unsigned int num,
			   unsigned int code);

	Stream_t *Direct;
	int fat_dirty;
	unsigned int fat_start;
	unsigned int fat_len;

	int num_fat;
	unsigned int end_fat;
	unsigned int last_fat;
	int fat_bits;
	struct FatMap_t *FatMap;

	int dir_start;
	int dir_len;
	int clus_start;

	int num_clus;
	char *drive; /* for error messages */

	/* fat 32 */
	unsigned int primaryFat;
	unsigned int writeAllFats;
	unsigned int rootCluster;
	int infoSectorLoc;
	unsigned int last; /* last sector allocated, or MAX32 if unknown */
	unsigned int freeSpace; /* free space, or MAX32 if unknown */
	int preallocatedClusters;

	int lastFatSectorNr;
	unsigned char *lastFatSectorData;
	fatAccessMode_t lastFatAccessMode;
	int sectorMask;
	int sectorShift;
} Fs_t;

int fs_free(Stream_t *Stream);

void set_fat12(Fs_t *Fs);
void set_fat16(Fs_t *Fs);
void set_fat32(Fs_t *Fs);
unsigned int get_next_free_cluster(Fs_t *Fs, unsigned int last);
unsigned int fatDecode(Fs_t *This, unsigned int pos);
void fatAppend(Fs_t *This, unsigned int pos, unsigned int newpos);
void fatDeallocate(Fs_t *This, unsigned int pos);
void fatAllocate(Fs_t *This, unsigned int pos, unsigned int value);
void fatEncode(Fs_t *This, unsigned int pos, unsigned int value);

int fat_read(Fs_t *This, struct bootsector *boot, int fat_bits,
			 size_t tot_sectors, int nodups);
void fat_write(Fs_t *This);
int zero_fat(Fs_t *Fs, int media_descriptor);
extern Class_t FsClass;
int fsPreallocateClusters(Fs_t *Fs, long);
Fs_t *getFs(Stream_t *Stream);


#endif

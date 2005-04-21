#ifndef MTOOLS_VFAT_H
#define MTOOLS_VFAT_H

#include "msdos.h"

/*
 * VFAT-related common header file
 */
#define VFAT_SUPPORT

struct unicode_char {
	char lchar;
	char uchar;
};


/* #define MAX_VFAT_SUBENTRIES 32 */ /* Theoretical max # of VSEs */
#define MAX_VFAT_SUBENTRIES 20		/* Max useful # of VSEs */
#define VSE_NAMELEN 13

#define VSE1SIZE 5
#define VSE2SIZE 6
#define VSE3SIZE 2

#include "stream.h"

struct vfat_subentry {
	unsigned char id;		/* 0x40 = last; & 0x1f = VSE ID */
	struct unicode_char text1[VSE1SIZE] PACKED;
	unsigned char attribute;	/* 0x0f for VFAT */
	unsigned char hash1;		/* Always 0? */
	unsigned char sum;		/* Checksum of short name */
	struct unicode_char text2[VSE2SIZE] PACKED;
	unsigned char sector_l;		/* 0 for VFAT */
	unsigned char sector_u;		/* 0 for VFAT */
	struct unicode_char text3[VSE3SIZE] PACKED;
};

/* Enough size for a worst case number of full VSEs plus a null */
#define VBUFSIZE ((MAX_VFAT_SUBENTRIES*VSE_NAMELEN) + 1)

/* Max legal length of a VFAT long name */
#define MAX_VNAMELEN (255)

#define VSE_PRESENT 0x01
#define VSE_LAST 0x40
#define VSE_MASK 0x1f

struct vfat_state {
	char name[VBUFSIZE];
	int status; /* is now a bit map of 32 bits */
	int subentries;
	unsigned char sum; /* no need to remember the sum for each entry,
			    * it is the same anyways */
	int present;
};


struct scan_state {
	int match_free;
	int shortmatch;
	int longmatch;
	int free_start;
	int free_end;
	int slot;
	int got_slots;
	int size_needed;
	int max_entry;
};

#include "mtoolsDirent.h"

void clear_vfat(struct vfat_state  *);
int unicode_write(char *, struct unicode_char *, int num, int *end);

int clear_vses(Stream_t *, int, size_t);
void autorename_short(char *, int);
void autorename_long(char *, int);

int lookupForInsert(Stream_t *Dir,
					char *dosname,
					char *longname,
					struct scan_state *ssp, 
					int ignore_entry,
					int source_entry,
					int pessimisticShortRename);

#define DO_OPEN 1 /* open all files that are found */
#define ACCEPT_LABEL 0x08
#define ACCEPT_DIR 0x10
#define ACCEPT_PLAIN 0x20
#define MATCH_ANY 0x40
#define NO_MSG 0x80
#define NO_DOTS 0x100 /* accept no dots if matched by wildcard */
#define DO_OPEN_DIRS 0x400 /* open all directories that are found */
#define OPEN_PARENT 0x1000  /* in target lookup, open parent
			     * instead of file itself */
#define NO_UNIX 0x2000 /* in target lookup, consider all files to reside on
			* the DOS fs */
#endif

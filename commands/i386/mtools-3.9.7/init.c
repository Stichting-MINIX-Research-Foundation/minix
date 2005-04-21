/*
 * Initialize an MSDOS diskette.  Read the boot sector, and switch to the
 * proper floppy disk device to match the format on the disk.  Sets a bunch
 * of global variables.  Returns 0 on success, or 1 on failure.
 */

#include "sysincludes.h"
#include "msdos.h"
#include "stream.h"
#include "mtools.h"
#include "fsP.h"
#include "plain_io.h"
#include "floppyd_io.h"
#include "xdf_io.h"
#include "buffer.h"

extern int errno;


#ifndef OS_Minix		/* Minix is memory starved. */
#define FULL_CYL
#endif

unsigned int num_clus;			/* total number of cluster */


/*
 * Read the boot sector.  We glean the disk parameters from this sector.
 */
static int read_boot(Stream_t *Stream, struct bootsector * boot, int size)
{	
	/* read the first sector, or part of it */
	if(!size)
		size = BOOTSIZE;
	if(size > 1024)
		size = 1024;

	if (force_read(Stream, (char *) boot, 0, size) != size)
		return -1;
	return 0;
}

static int fs_flush(Stream_t *Stream)
{
	DeclareThis(Fs_t);

	fat_write(This);
	return 0;
}

Class_t FsClass = {
	read_pass_through, /* read */
	write_pass_through, /* write */
	fs_flush, 
	fs_free, /* free */
	0, /* set geometry */
	get_data_pass_through,
	0 /* pre allocate */
};

static int get_media_type(Stream_t *St, struct bootsector *boot)
{
	int media;

	media = boot->descr;
	if(media < 0xf0){
		char temp[512];
		/* old DOS disk. Media descriptor in the first FAT byte */
		/* old DOS disk always have 512-byte sectors */
		if (force_read(St,temp,(mt_off_t) 512,512) == 512)
			media = (unsigned char) temp[0];
		else
			media = 0;
	} else
		media += 0x100;
	return media;
}


Stream_t *GetFs(Stream_t *Fs)
{
	while(Fs && Fs->Class != &FsClass)
		Fs = Fs->Next;
	return Fs;
}

Stream_t *find_device(char *drive, int mode, struct device *out_dev,
		      struct bootsector *boot,
		      char *name, int *media, mt_size_t *maxSize)
{
	char errmsg[200];
	Stream_t *Stream;
	struct device *dev;
	int r;
#ifdef OS_Minix
	static char *devname;
	struct device onedevice[2];
	struct stat stbuf;

	free(devname);
	devname = safe_malloc((9 + strlen(drive)) * sizeof(devname[0]));
	strcpy(devname, "/dev/dosX");
	if (isupper(drive[0]) && drive[1] == 0) {
		/* single letter device name, use /dev/dos$drive */
		devname[8]= drive[0];
	} else
	if (strchr(drive, '/') == NULL) {
		/* a simple name, use /dev/$drive */
		strcpy(devname+5, drive);
	} else {
		/* a pathname, use as is. */
		strcpy(devname, drive);
	}
	if (stat(devname, &stbuf) != -1) {
		memset(onedevice, 0, sizeof(onedevice));
		onedevice[0].name = devname;
		onedevice[0].drive = drive;
		onedevice[1].name = NULL;
		onedevice[1].drive = NULL;
		dev = onedevice;
	} else {
		dev = devices;
	}
#else
	dev = devices;
#endif

	Stream = NULL;
	sprintf(errmsg, "Drive '%s:' not supported", drive);	
					/* open the device */
	for (; dev->name; dev++) {
		FREE(&Stream);
		if (strcmp(dev->drive, drive) != 0)
			continue;
		*out_dev = *dev;
		expand(dev->name,name);
#ifdef USING_NEW_VOLD
		strcpy(name, getVoldName(dev, name));
#endif

		Stream = 0;
#ifdef USE_XDF
		Stream = XdfOpen(out_dev, name, mode, errmsg, 0);
		if(Stream) {
			out_dev->use_2m = 0x7f;
			if(maxSize)
			    *maxSize = max_off_t_31;
		}
#endif

#ifdef USE_FLOPPYD
		if(!Stream) {
			Stream = FloppydOpen(out_dev, dev, name, mode, errmsg, 0, 1);
			if(Stream && maxSize)
				*maxSize = max_off_t_31;
		}
#endif

		if (!Stream)
			Stream = SimpleFileOpen(out_dev, dev, name, mode,
						errmsg, 0, 1, maxSize);

		if( !Stream)
			continue;

		/* read the boot sector */
		if ((r=read_boot(Stream, boot, out_dev->blocksize)) < 0){
			sprintf(errmsg,
				"init %s: could not read boot sector",
				drive);
			continue;
		}

		if((*media= get_media_type(Stream, boot)) <= 0xf0 ){
			if (boot->jump[2]=='L') 
				sprintf(errmsg,
					"diskette %s: is Linux LILO, not DOS", 
					drive);
			else 
				sprintf(errmsg,"init %s: non DOS media", drive);
			continue;
		}

		/* set new parameters, if needed */
		errno = 0;
		if(SET_GEOM(Stream, out_dev, dev, *media, boot)){
			if(errno)
#ifdef HAVE_SNPRINTF
				snprintf(errmsg, 199,
					"Can't set disk parameters for %s: %s", 
					drive, strerror(errno));
#else
				sprintf(errmsg,
					"Can't set disk parameters for %s: %s", 
					drive, strerror(errno));
#endif
			else
				sprintf(errmsg, 
					"Can't set disk parameters for %s", 
					drive);
			continue;
		}
		break;
	}

	/* print error msg if needed */	
	if ( dev->drive == 0 ){
		FREE(&Stream);
		fprintf(stderr,"%s\n",errmsg);
		return NULL;
	}
#ifdef OS_Minix
	/* Minix can lseek up to 4G. */
	if (maxSize) *maxSize = 0xFFFFFFFFUL;
#endif
	return Stream;
}


Stream_t *fs_init(char *drive, int mode)
{
	int blocksize;
	int media,i;
	int nhs;
	int disk_size = 0;	/* In case we don't happen to set this below */
	size_t tot_sectors;
	char name[EXPAND_BUF];
	int cylinder_size;
	struct device dev;
	mt_size_t maxSize;

	struct bootsector boot0;
#define boot (&boot0)
	Fs_t *This;

	This = New(Fs_t);
	if (!This)
		return NULL;

	This->Direct = NULL;
	This->Next = NULL;
	This->refs = 1;
	This->Buffer = 0;
	This->Class = &FsClass;
	This->preallocatedClusters = 0;
	This->lastFatSectorNr = 0;
	This->lastFatAccessMode = 0;
	This->lastFatSectorData = 0;
	This->drive = drive;
	This->last = 0;

	This->Direct = find_device(drive, mode, &dev, &boot0, name, &media, 
							   &maxSize);
	if(!This->Direct)
		return NULL;
	
	This->sector_size = WORD(secsiz);
	if(This->sector_size > MAX_SECTOR){
		fprintf(stderr,"init %s: sector size too big\n", drive);
		return NULL;
	}

	i = log_2(This->sector_size);

	if(i == 24) {
		fprintf(stderr, 
			"init %c: sector size (%d) not a small power of two\n",
			drive, This->sector_size);
		return NULL;
	}
	This->sectorShift = i;
	This->sectorMask = This->sector_size - 1;


	cylinder_size = dev.heads * dev.sectors;
	if (!tot_sectors) tot_sectors = dev.tracks * cylinder_size;

	This->serialized = 0;
	if ((media & ~7) == 0xf8){
		i = media & 3;
		This->cluster_size = old_dos[i].cluster_size;
		tot_sectors = cylinder_size * old_dos[i].tracks;
		This->fat_start = 1;
		This->fat_len = old_dos[i].fat_len;
		This->dir_len = old_dos[i].dir_len;
		This->num_fat = 2;
		This->sector_size = 512;
		This->sectorShift = 9;
		This->sectorMask = 511;
		This->fat_bits = 12;
		nhs = 0;
	} else {
		struct label_blk_t *labelBlock;
		/*
		 * all numbers are in sectors, except num_clus 
		 * (which is in clusters)
		 */
		tot_sectors = WORD(psect);
		if(!tot_sectors) {
			tot_sectors = DWORD(bigsect);			
			nhs = DWORD(nhs);
		} else
			nhs = WORD(nhs);


		This->cluster_size = boot0.clsiz; 		
		This->fat_start = WORD(nrsvsect);
		This->fat_len = WORD(fatlen);
		This->dir_len = WORD(dirents) * MDIR_SIZE / This->sector_size;
		This->num_fat = boot0.nfat;

		if (This->fat_len) {
			labelBlock = &boot0.ext.old.labelBlock;
		} else {
			labelBlock = &boot0.ext.fat32.labelBlock;
		}

		if(labelBlock->dos4 == 0x29) {
			This->serialized = 1;
			This->serial_number = _DWORD(labelBlock->serial);
		}
	}

	if (tot_sectors >= (maxSize >> This->sectorShift)) {
		fprintf(stderr, "Big disks not supported on this architecture\n");
		exit(1);
	}

#ifndef OS_Minix   /* Strange check, MS-DOS isn't that picky. */

	if(!mtools_skip_check && (tot_sectors % dev.sectors)){
		fprintf(stderr,
			"Total number of sectors not a multiple of"
			" sectors per track!\n");
		fprintf(stderr,
			"Add mtools_skip_check=1 to your .mtoolsrc file "
			"to skip this test\n");
		exit(1);
	}
#endif

	/* full cylinder buffering */
#ifdef FULL_CYL
	disk_size = (dev.tracks) ? cylinder_size : 512;
#else /* FULL_CYL */
	disk_size = (dev.tracks) ? dev.sectors : 512;
#endif /* FULL_CYL */

#if (defined OS_sysv4 && !defined OS_solaris)
	/*
	 * The driver in Dell's SVR4 v2.01 is unreliable with large writes.
	 */
        disk_size = 0;
#endif /* (defined sysv4 && !defined(solaris)) */

#ifdef OS_linux
	disk_size = cylinder_size;
#endif

#if 1
	if(disk_size > 256) {
		disk_size = dev.sectors;
		if(dev.sectors % 2)
			disk_size <<= 1;
	}
#endif
	if (disk_size % 2)
		disk_size *= 2;

	if(!dev.blocksize || dev.blocksize < This->sector_size)
		blocksize = This->sector_size;
	else
		blocksize = dev.blocksize;
	if (disk_size)
		This->Next = buf_init(This->Direct,
				      8 * disk_size * blocksize,
				      disk_size * blocksize,
				      This->sector_size);
	else
		This->Next = This->Direct;

	if (This->Next == NULL) {
		perror("init: allocate buffer");
		This->Next = This->Direct;
	}

	/* read the FAT sectors */
	if(fat_read(This, &boot0, dev.fat_bits, tot_sectors, dev.use_2m&0x7f)){
		This->num_fat = 1;
		FREE(&This->Next);
		Free(This->Next);
		return NULL;
	}
	return (Stream_t *) This;
}

char *getDrive(Stream_t *Stream)
{
	DeclareThis(Fs_t);

	if(This->Class != &FsClass)
		return getDrive(GetFs(Stream));
	else
		return This->drive;
}

int fsPreallocateClusters(Fs_t *Fs, long size)
{
	if(size > 0 && getfreeMinClusters((Stream_t *)Fs, size) != 1)
		return -1;

	Fs->preallocatedClusters += size;
	return 0;
}

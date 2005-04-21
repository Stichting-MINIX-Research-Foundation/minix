/*
 * mlabel.c
 * Make an MSDOS volume label
 */

#include "sysincludes.h"
#include "msdos.h"
#include "mainloop.h"
#include "vfat.h"
#include "mtools.h"
#include "nameclash.h"

static void usage(void)
{
	fprintf(stderr, 
		"Mtools version %s, dated %s\n", mversion, mdate);
	fprintf(stderr, 
		"Usage: %s [-v] drive\n\t-v Verbose\n", progname);
	exit(1);
}


static void displayInfosector(Stream_t *Stream, struct bootsector *boot)
{
	InfoSector_t *infosec;

	if(WORD(ext.fat32.infoSector) == MAX32)
		return;

	infosec = (InfoSector_t *) safe_malloc(WORD(secsiz));
	force_read(Stream, (char *) infosec, 
			   (mt_off_t) WORD(secsiz) * WORD(ext.fat32.infoSector),
			   WORD(secsiz));
	printf("\nInfosector:\n");
	printf("signature=0x%08x\n", _DWORD(infosec->signature1));
	if(_DWORD(infosec->count) != MAX32)
		printf("free clusters=%u\n", _DWORD(infosec->count));
	if(_DWORD(infosec->pos) != MAX32)
		printf("last allocated cluster=%u\n", _DWORD(infosec->pos));
}


void minfo(int argc, char **argv, int type)
{
	struct bootsector boot0;
#define boot (&boot0)
	char name[EXPAND_BUF];
	int media;
	int tot_sectors;
	struct device dev;
	char *drive;
	int verbose=0;
	int c;
	Stream_t *Stream;
	struct label_blk_t *labelBlock;
	
	while ((c = getopt(argc, argv, "v")) != EOF) {
		switch (c) {
			case 'v':
				verbose = 1;
				break;
			default:
				usage();
		}
	}

	if(argc == optind)
		usage();

	for(;optind < argc; optind++) {
		if(skip_drive(argv[optind]) == argv[optind])
			usage();
		drive = get_drive(argv[optind], NULL);

		if(! (Stream = find_device(drive, O_RDONLY, &dev, boot, 
					   name, &media, 0)))
			exit(1);

		tot_sectors = DWORD(bigsect);
		SET_INT(tot_sectors, WORD(psect));
		printf("device information:\n");
		printf("===================\n");
		printf("filename=\"%s\"\n", name);
		printf("sectors per track: %d\n", dev.sectors);
		printf("heads: %d\n", dev.heads);
		printf("cylinders: %d\n\n", dev.tracks);
		printf("mformat command line: mformat -t %d -h %d -s %d ",
		       dev.tracks, dev.heads, dev.sectors);
		if(DWORD(nhs))
			printf("-H %d ", DWORD(nhs));
		printf("%s:\n", drive);
		printf("\n");
		
		printf("bootsector information\n");
		printf("======================\n");
		printf("banner:\"%8s\"\n", boot->banner);
		printf("sector size: %d bytes\n", WORD(secsiz));
		printf("cluster size: %d sectors\n", boot->clsiz);
		printf("reserved (boot) sectors: %d\n", WORD(nrsvsect));
		printf("fats: %d\n", boot->nfat);
		printf("max available root directory slots: %d\n", 
		       WORD(dirents));
		printf("small size: %d sectors\n", WORD(psect));
		printf("media descriptor byte: 0x%x\n", boot->descr);
		printf("sectors per fat: %d\n", WORD(fatlen));
		printf("sectors per track: %d\n", WORD(nsect));
		printf("heads: %d\n", WORD(nheads));
		printf("hidden sectors: %d\n", DWORD(nhs));
		printf("big size: %d sectors\n", DWORD(bigsect));

		if(WORD(fatlen)) {
		    labelBlock = &boot->ext.old.labelBlock;
		} else {
		    labelBlock = &boot->ext.fat32.labelBlock;
		}

		printf("physical drive id: 0x%x\n", 
		       labelBlock->physdrive);
		printf("reserved=0x%x\n", 
		       labelBlock->reserved);
		printf("dos4=0x%x\n", 
		       labelBlock->dos4);
		printf("serial number: %08X\n", 
		       _DWORD(labelBlock->serial));
		printf("disk label=\"%11.11s\"\n", 
		       labelBlock->label);
		printf("disk type=\"%8.8s\"\n", 
		       labelBlock->fat_type);

		if(!WORD(fatlen)){
			printf("Big fatlen=%u\n",
			       DWORD(ext.fat32.bigFat));
			printf("Extended flags=0x%04x\n",
			       WORD(ext.fat32.extFlags));
			printf("FS version=0x%04x\n",
			       WORD(ext.fat32.fsVersion));
			printf("rootCluster=%u\n",
			       DWORD(ext.fat32.rootCluster));
			if(WORD(ext.fat32.infoSector) != MAX32)
				printf("infoSector location=%d\n",
				       WORD(ext.fat32.infoSector));
			if(WORD(ext.fat32.backupBoot) != MAX32)
				printf("backup boot sector=%d\n",
				       WORD(ext.fat32.backupBoot));
			displayInfosector(Stream,boot);
		}

		if(verbose) {
			int size;
			unsigned char *buf;

			printf("\n");
			size = WORD(secsiz);
			
			buf = (unsigned char *) malloc(size);
			if(!buf) {
				fprintf(stderr, "Out of memory error\n");
				exit(1);
			}

			size = READS(Stream, buf, (mt_off_t) 0, size);
			if(size < 0) {
				perror("read boot sector");
				exit(1);
			}

			print_sector("Boot sector hexdump", buf, size);
		}
	}

	exit(0);
}

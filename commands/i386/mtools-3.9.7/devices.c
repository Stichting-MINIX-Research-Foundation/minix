/*
 * This file is modified to perform on the UXP/DS operating system 
 * by FUJITSU Limited on 1996.6.4
 */

/*
 * Device tables.  See the Configure file for a complete description.
 */

#define NO_TERMIO
#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "devices.h"

#define INIT_NOOP

#define DEF_ARG1(x) (x), 0x2,0,(char *)0, 0, 0
#define DEF_ARG0(x) 0,DEF_ARG1(x)

#define MDEF_ARG 0L,DEF_ARG0(MFORMAT_ONLY_FLAG)
#define FDEF_ARG 0L,DEF_ARG0(0)
#define VOLD_DEF_ARG 0L,DEF_ARG0(VOLD_FLAG|MFORMAT_ONLY_FLAG)

#define MED312	12,0,80,2,36,0,MDEF_ARG /* 3 1/2 extra density */
#define MHD312	12,0,80,2,18,0,MDEF_ARG /* 3 1/2 high density */
#define MDD312	12,0,80,2, 9,0,MDEF_ARG /* 3 1/2 double density */
#define MHD514	12,0,80,2,15,0,MDEF_ARG /* 5 1/4 high density */
#define MDD514	12,0,40,2, 9,0,MDEF_ARG /* 5 1/4 double density (360k) */
#define MSS514	12,0,40,1, 9,0,MDEF_ARG /* 5 1/4 single sided DD, (180k) */
#define MDDsmall	12,0,40,2, 8,0,MDEF_ARG /* 5 1/4 double density (320k) */
#define MSSsmall	12,0,40,1, 8,0,MDEF_ARG /* 5 1/4 single sided DD, (160k) */

#define FED312	12,0,80,2,36,0,FDEF_ARG /* 3 1/2 extra density */
#define FHD312	12,0,80,2,18,0,FDEF_ARG /* 3 1/2 high density */
#define FDD312	12,0,80,2, 9,0,FDEF_ARG /* 3 1/2 double density */
#define FHD514	12,0,80,2,15,0,FDEF_ARG /* 5 1/4 high density */
#define FDD514	12,0,40,2, 9,0,FDEF_ARG /* 5 1/4 double density (360k) */
#define FSS514	12,0,40,1, 9,0,FDEF_ARG /* 5 1/4 single sided DD, (180k) */
#define FDDsmall	12,0,40,2, 8,0,FDEF_ARG /* 5 1/4 double density (320k) */
#define FSSsmall	12,0,40,1, 8,0,FDEF_ARG /* 5 1/4 single sided DD, (160k) */

#define GENHD	16,0, 0,0, 0,0,MDEF_ARG /* Generic 16 bit FAT fs */
#define GENFD	12,0,80,2,18,0,MDEF_ARG /* Generic 12 bit FAT fs */
#define VOLDFD	12,0,80,2,18,0,VOLD_DEF_ARG /* Generic 12 bit FAT fs with vold */
#define GEN    	 0,0, 0,0, 0,0,MDEF_ARG /* Generic fs of any FAT bits */

#define ZIPJAZ(x,c,h,s,y) 16,(x),(c),(h),(s),(s),0L, 4, \
		DEF_ARG1((y)|MFORMAT_ONLY_FLAG) /* Jaz disks */

#define JAZ(x)	 ZIPJAZ(x,1021, 64, 32, 0)
#define RJAZ(x)	 ZIPJAZ(x,1021, 64, 32, SCSI_FLAG|PRIV_FLAG)
#define ZIP(x)	 ZIPJAZ(x,96, 64, 32, 0)
#define RZIP(x)	 ZIPJAZ(x,96, 64, 32, SCSI_FLAG|PRIV_FLAG)

#define REMOTE    {"$DISPLAY", 'X', 0,0, 0,0, 0,0,0L, DEF_ARG0(FLOPPYD_FLAG)}



#if defined(INIT_GENERIC) || defined(INIT_NOOP)
static int compare_geom(struct device *dev, struct device *orig_dev)
{
	if(IS_MFORMAT_ONLY(orig_dev))
		return 0; /* geometry only for mformatting ==> ok */
	if(!orig_dev || !orig_dev->tracks || !dev || !dev->tracks)
		return 0; /* no original device. This is ok */
	return(orig_dev->tracks != dev->tracks ||
	       orig_dev->heads != dev->heads ||
	       orig_dev->sectors  != dev->sectors);
}
#endif

#define devices const_devices


#ifdef OS_aux
#define predefined_devices
struct device devices[] = {
   {"/dev/floppy0", "A", GENFD },
   {"/dev/rdsk/c104d0s31", "J", JAZ(O_EXCL) },
   {"/dev/rdsk/c105d0s31", "Z", ZIP(O_EXCL) },
   REMOTE
};
#endif /* aux */


#ifdef OS_lynxos
#define predefined_devices
struct device devices[] = {
	{"/dev/fd1440.0", 	"A", MHD312 },
	REMOTE
};
#endif


#ifdef __BEOS__
#define predefined_devices
struct device devices[] = {
	{"/dev/disk/floppy/raw", 	"A", MHD312 },
	REMOTE
};
#endif /* BEBOX */


#ifdef OS_hpux

#define predefined_devices
struct device devices[] = {
#ifdef OS_hpux10
/* hpux10 uses different device names according to Frank Maritato
 * <frank@math.hmc.edu> */
	{"/dev/floppy/c0t0d0",		"A", MHD312 },
	{"/dev/floppy/c0t0d1",		"B", MHD312 }, /* guessed by me */
 	{"/dev/rscsi",			"C", GENHD }, /* guessed by me */
#else
/* Use rfloppy, according to Simao Campos <simao@iris.ctd.comsat.com> */
	{"/dev/rfloppy/c201d0s0",	"A", FHD312 },
	{"/dev/rfloppy/c20Ad0s0", 	"A", FHD312 },
 	{"/dev/rfloppy/c201d1s0",	"B", FHD312 },
 	{"/dev/rfloppy/c20Ad1s0",	"B", FHD312 },
 	{"/dev/rscsi",			"C", GENHD },
#endif
	{"/dev/rdsk/c201d4",		"J", RJAZ(O_EXCL) },
	{"/dev/rdsk/c201d4s0",		"J", RJAZ(O_EXCL) },
	{"/dev/rdsk/c201d5",		"Z", RZIP(O_EXCL) },
	{"/dev/rdsk/c201d5s0",		"Z", RZIP(O_EXCL) },
	REMOTE
};

#ifdef HAVE_SYS_FLOPPY
/* geometry setting ioctl's contributed by Paolo Zeppegno
 * <paolo@to.sem.it>, may cause "Not a typewriter" messages on other
 * versions according to support@vital.com */

#include <sys/floppy.h>
#undef SSIZE

struct generic_floppy_struct
{
  struct floppy_geometry fg;
};

#define BLOCK_MAJOR 24
#define CHAR_MAJOR 112

static inline int get_parameters(int fd, struct generic_floppy_struct *floppy)
{
	if (ioctl(fd, FLOPPY_GET_GEOMETRY, &(floppy->fg)) != 0) {
		perror("FLOPPY_GET_GEOMETRY");
		return(1);
	}
	
	return 0;
}

#define TRACKS(floppy) floppy.fg.tracks
#define HEADS(floppy) floppy.fg.heads
#define SECTORS(floppy) floppy.fg.sectors
#define FD_SECTSIZE(floppy) floppy.fg.sector_size
#define FD_SET_SECTSIZE(floppy,v) { floppy.fg.sector_size = v; }

static inline int set_parameters(int fd, struct generic_floppy_struct *floppy, 
				 struct stat *buf)
{
	if (ioctl(fd, FLOPPY_SET_GEOMETRY, &(floppy->fg)) != 0) {
		perror("");
		return(1);
	}
	
	return 0;
}
#define INIT_GENERIC
#endif

#endif /* hpux */
 

#if (defined(OS_sinix) || defined(VENDOR_sni) || defined(SNI))
#define predefined_devices
struct device devices[] = {
#ifdef CPU_mips     /* for Siemens Nixdorf's  SINIX-N/O (mips) 5.4x SVR4 */
	{ "/dev/at/flp/f0t",    "A", FHD312},
	{ "/dev/fd0",           "A", GENFD},
#else
#ifdef CPU_i386     /* for Siemens Nixdorf's  SINIX-D/L (intel) 5.4x SVR4 */
	{ "/dev/fd0135ds18",	"A", FHD312},
	{ "/dev/fd0135ds9",	"A", FDD312},
	{ "/dev/fd0",		"A", GENFD},
	{ "/dev/fd1135ds15",	"B", FHD514},
	{ "/dev/fd1135ds9",	"B", FDD514},
	{ "/dev/fd1",		"B", GENFD},
#endif /* CPU_i386 */
#endif /*mips*/
	REMOTE
};
#endif

#ifdef OS_ultrix
#define predefined_devices
struct device devices[] = {
	{"/dev/rfd0a",		"A", GENFD}, /* guessed */
	{"/dev/rfd0c",		"A", GENFD}, /* guessed */
	REMOTE
};

#endif


#ifdef OS_isc
#define predefined_devices
#if (defined(OS_isc2) && defined(OLDSTUFF))
struct device devices[] = {
	{"/dev/rdsk/f0d9dt",   	"A", FDD514},
	{"/dev/rdsk/f0q15dt",	"A", FHD514},
	{"/dev/rdsk/f0d8dt",	"A", FDDsmall},
	{"/dev/rdsk/f13ht",	"B", FHD312},
	{"/dev/rdsk/f13dt",	"B", FDD312},
	{"/dev/rdsk/0p1",	"C", GENHD},
	{"/usr/vpix/defaults/C:","D",12, 0, 0, 0, 0,8704L,DEF_ARG0},
	{"$HOME/vpix/C:", 	"E", 12, 0, 0, 0, 0,8704L,MDEF_ARG},
	REMOTE
};
#else
/* contributed by larry.jones@sdrc.com (Larry Jones) */
struct device devices[] = {
	{"/dev/rfd0",		"A", GEN},
	{"/dev/rfd1",		"B", GEN},
	{"/dev/rdsk/0p1",	"C", GEN},
	{"/usr/vpix/defaults/C:","D", GEN, 1},
	{"$HOME/vpix/C:", 	"E", GEN, 1},
	REMOTE
};

#include <sys/vtoc.h>
#include <sys/sysmacros.h>
#undef SSIZE
#define BLOCK_MAJOR 1
#define CHAR_MAJOR  1
#define generic_floppy_struct disk_parms
int ioctl(int, int, void *);

static int get_parameters(int fd, struct generic_floppy_struct *floppy)
{
	mt_off_t off;
	char buf[512];

	off = lseek(fd, 0, SEEK_CUR);
	if(off < 0) {
		perror("device seek 1");
		exit(1);
	}
	if (off == 0) {
		/* need to read at least 1 sector to get correct info */
		read(fd, buf, sizeof buf);
		if(lseek(fd, 0, SEEK_SET) < 0) {
			perror("device seek 2");
			exit(1);
		}
	}
	return ioctl(fd, V_GETPARMS, floppy);
}

#define TRACKS(floppy)  (floppy).dp_cyls
#define HEADS(floppy)   (floppy).dp_heads
#define SECTORS(floppy) (floppy).dp_sectors
#define FD_SECTSIZE(floppy) (floppy).dp_secsiz
#define FD_SET_SECTSIZE(floppy,v) { (floppy).dp_secsiz = (v); }

static int set_parameters(int fd, struct generic_floppy_struct *floppy,
	struct stat *buf)
{
	return 1;
}

#define INIT_GENERIC
#endif
#endif /* isc */

#ifdef CPU_i370
#define predefined_devices
struct device devices[] = {
	{"/dev/rfd0", "A", GENFD},
	REMOTE
};
#endif /* CPU_i370 */

#ifdef OS_aix
/* modified by Federico Bianchi */
#define predefined_devices
struct device devices[] = {
	{"/dev/fd0","A",GENFD},
	REMOTE
};
#endif /* aix */

  
#ifdef OS_osf4
/* modified by Chris Samuel <chris@rivers.dra.hmg.gb> */
#define predefined_devices
struct device devices[] = {
	{"/dev/fd0c","A",GENFD},
	REMOTE
};
#endif /* OS_osf4 */


#ifdef OS_solaris

#ifdef USING_NEW_VOLD

char *alias_name = NULL;
  
extern char *media_oldaliases(char *);
extern char *media_findname(char *);

char *getVoldName(struct device *dev, char *name)
{
	char *rname;
  
	if(!SHOULD_USE_VOLD(dev))
		return name;

	/***
	 * Solaris specific routines to use the volume management
	 * daemon and libraries to get the correct device name...
	 ***/
	rname = media_findname(name);
#ifdef HAVE_MEDIA_OLDALIASES
	if (rname == NULL) {
		if ((alias_name = media_oldaliases(name)) != NULL)
			rname = media_findname(alias_name);
	}
#endif
	if (rname == NULL) {
		fprintf(stderr, 
				"No such volume or no media in device: %s.\n", 
				name);
		exit(1);
	}
	return rname;
}
#endif /* USING_NEW_VOLD */

#define predefined_devices
struct device devices[] = {
#ifdef  USING_NEW_VOLD
	{"floppy", "A", VOLDFD },
#elif	USING_VOLD
	{"/vol/dev/aliases/floppy0", "A", GENFD},
	{"/dev/rdiskette", "B", GENFD},
#else	/* ! USING_VOLD */
	{"/dev/rdiskette", "A", GENFD},
	{"/vol/dev/aliases/floppy0", "B", GENFD},
#endif	/* USING_VOLD */
	{"/dev/rdsk/c0t4d0s2", "J", RJAZ(O_NDELAY)},
	{"/dev/rdsk/c0t5d0s2", "Z", RZIP(O_NDELAY)},
	REMOTE
};



/*
 * Ofer Licht <ofer@stat.Berkeley.EDU>, May 14, 1997.
 */

#define INIT_GENERIC

#include <sys/fdio.h>
#include <sys/mkdev.h>	/* for major() */

struct generic_floppy_struct
{
  struct fd_char fdchar;
};

#define BLOCK_MAJOR 36
#define CHAR_MAJOR 36

static inline int get_parameters(int fd, struct generic_floppy_struct *floppy)
{
	if (ioctl(fd, FDIOGCHAR, &(floppy->fdchar)) != 0) {
		perror("");
		ioctl(fd, FDEJECT, NULL);
		return(1);
	}
	return 0;
}

#define TRACKS(floppy) floppy.fdchar.fdc_ncyl
#define HEADS(floppy) floppy.fdchar.fdc_nhead
#define SECTORS(floppy) floppy.fdchar.fdc_secptrack
/* SECTORS_PER_DISK(floppy) not used */
#define FD_SECTSIZE(floppy) floppy.fdchar.fdc_sec_size
#define FD_SET_SECTSIZE(floppy,v) { floppy.fdchar.fdc_sec_size = v; }

static inline int set_parameters(int fd, struct generic_floppy_struct *floppy, 
				 struct stat *buf)
{
	if (ioctl(fd, FDIOSCHAR, &(floppy->fdchar)) != 0) {
		ioctl(fd, FDEJECT, NULL);
		perror("");
		return(1);
	}
	return 0;
}
#define INIT_GENERIC
#endif /* solaris */

#ifdef OS_sunos3
#define predefined_devices
struct device devices[] = {
	{"/dev/rfdl0c",	"A", FDD312},
	{"/dev/rfd0c",	"A", FHD312},
	REMOTE
};
#endif /* OS_sunos3 */

#ifdef OS_xenix
#define predefined_devices
struct device devices[] = {
	{"/dev/fd096ds15",	"A", FHD514},
	{"/dev/fd048ds9",	"A", FDD514},
	{"/dev/fd1135ds18",	"B", FHD312},
	{"/dev/fd1135ds9",	"B", FDD312},
	{"/dev/hd0d",		"C", GENHD},
	REMOTE
};
#endif /* OS_xenix */

#ifdef OS_sco
#define predefined_devices
struct device devices[] = {
	{ "/dev/fd0135ds18",	"A", FHD312},
	{ "/dev/fd0135ds9",	"A", FDD312},
	{ "/dev/fd0",		"A", GENFD},
	{ "/dev/fd1135ds15",	"B", FHD514},
	{ "/dev/fd1135ds9",	"B", FDD514},
	{ "/dev/fd1",		"B", GENFD},
	{ "/dev/hd0d",		"C", GENHD},
	REMOTE
};
#endif /* OS_sco */


#ifdef OS_irix
#define predefined_devices
struct device devices[] = {
  { "/dev/rdsk/fds0d2.3.5hi",	"A", FHD312},
  { "/dev/rdsk/fds0d2.3.5",	"A", FDD312},
  { "/dev/rdsk/fds0d2.96",	"A", FHD514},
  {"/dev/rdsk/fds0d2.48",	"A", FDD514},
  REMOTE
};
#endif /* OS_irix */


#ifdef OS_sunos4
#include <sys/ioctl.h>
#include <sun/dkio.h>

#define predefined_devices
struct device devices[] = {
	{"/dev/rfd0c",	"A", GENFD},
	{"/dev/rsd4c",	"J", RJAZ(O_NDELAY)},
	{"/dev/rsd5c",	"Z", RZIP(O_NDELAY)},
	REMOTE
};

/*
 * Stuffing back the floppy parameters into the driver allows for gems
 * like 10 sector or single sided floppies from Atari ST systems.
 * 
 * Martin Schulz, Universite de Moncton, N.B., Canada, March 11, 1991.
 */

#define INIT_GENERIC

struct generic_floppy_struct
{
  struct fdk_char dkbuf;
  struct dk_map dkmap;
};

#define BLOCK_MAJOR 16
#define CHAR_MAJOR 54

static inline int get_parameters(int fd, struct generic_floppy_struct *floppy)
{
	if (ioctl(fd, DKIOCGPART, &(floppy->dkmap)) != 0) {
		perror("DKIOCGPART");
		ioctl(fd, FDKEJECT, NULL);
		return(1);
	}
	
	if (ioctl(fd, FDKIOGCHAR, &( floppy->dkbuf)) != 0) {
		perror("");
		ioctl(fd, FDKEJECT, NULL);
		return(1);
	}
	return 0;
}

#define TRACKS(floppy) floppy.dkbuf.ncyl
#define HEADS(floppy) floppy.dkbuf.nhead
#define SECTORS(floppy) floppy.dkbuf.secptrack
#define SECTORS_PER_DISK(floppy) floppy.dkmap.dkl_nblk
#define FD_SECTSIZE(floppy) floppy.dkbuf.sec_size
#define FD_SET_SECTSIZE(floppy,v) { floppy.dkbuf.sec_size = v; }

static inline int set_parameters(int fd, struct generic_floppy_struct *floppy, 
				 struct stat *buf)
{
	if (ioctl(fd, FDKIOSCHAR, &(floppy->dkbuf)) != 0) {
		ioctl(fd, FDKEJECT, NULL);
		perror("");
		return(1);
	}
	
	if (ioctl(fd, ( unsigned int) DKIOCSPART, &(floppy->dkmap)) != 0) {
		ioctl(fd, FDKEJECT, NULL);
		perror("");
		return(1);
	}
	return 0;
}
#define INIT_GENERIC
#endif /* sparc && sunos */


#ifdef DPX1000
#define predefined_devices
struct device devices[] = {
	/* [block device]: DPX1000 has /dev/flbm60, DPX2 has /dev/easyfb */
	{"/dev/flbm60", "A", MHD514};
	{"/dev/flbm60", "B", MDD514},
	{"/dev/flbm60", "C", MDDsmall},
	{"/dev/flbm60", "D", MSS},
	{"/dev/flbm60", "E", MSSsmall},
	REMOTE
};
#endif /* DPX1000 */

#ifdef OS_bosx
#define predefined_devices
struct device devices[] = {
	/* [block device]: DPX1000 has /dev/flbm60, DPX2 has /dev/easyfb */
	{"/dev/easyfb", "A", MHD514},
	{"/dev/easyfb", "B", MDD514},
	{"/dev/easyfb", "C", MDDsmall},
	{"/dev/easyfb", "D", MSS},
	{"/dev/easyfb", "E", MSSsmall},
	REMOTE
};
#endif /* OS_bosx */

#ifdef OS_linux

const char *error_msg[22]={
"Missing Data Address Mark",
"Bad cylinder",
"Scan not satisfied",
"Scan equal hit",
"Wrong cylinder",
"CRC error in data field",
"Control Mark = deleted",
0,

"Missing Address Mark",
"Write Protect",
"No Data - unreadable",
0,
"OverRun",
"CRC error in data or address",
0,
"End Of Cylinder",

0,
0,
0,
"Not ready",
"Equipment check error",
"Seek end" };


static inline void print_message(RawRequest_t *raw_cmd,const char *message)
{
	int i, code;
	if(!message)
		return;

	fprintf(stderr,"   ");
	for (i=0; i< raw_cmd->cmd_count; i++)
		fprintf(stderr,"%2.2x ", 
			(int)raw_cmd->cmd[i] );
	fprintf(stderr,"\n");
	for (i=0; i< raw_cmd->reply_count; i++)
		fprintf(stderr,"%2.2x ",
			(int)raw_cmd->reply[i] );
	fprintf(stderr,"\n");
	code = (raw_cmd->reply[0] <<16) + 
		(raw_cmd->reply[1] << 8) + 
		raw_cmd->reply[2];
	for(i=0; i<22; i++){
		if ((code & (1 << i)) && error_msg[i])
			fprintf(stderr,"%s\n",
				error_msg[i]);
	}
}


/* return values:
 *  -1: Fatal error, don't bother retrying.
 *   0: OK
 *   1: minor error, retry
 */

int send_one_cmd(int fd, RawRequest_t *raw_cmd, const char *message)
{
	if (ioctl( fd, FDRAWCMD, raw_cmd) >= 0) {
		if (raw_cmd->reply_count < 7) {
			fprintf(stderr,"Short reply from FDC\n");
			return -1;
		}		
		return 0;
	}

	switch(errno) {
		case EBUSY:
			fprintf(stderr, "FDC busy, sleeping for a second\n");
			sleep(1);
			return 1;
		case EIO:
			fprintf(stderr,"resetting controller\n");
			if(ioctl(fd, FDRESET, 2)  < 0){
				perror("reset");
				return -1;
			}
			return 1;
		default:
			perror(message);
			return -1;
	}
}


/*
 * return values
 *  -1: error
 *   0: OK, last sector
 *   1: more raw commands follow
 */

int analyze_one_reply(RawRequest_t *raw_cmd, int *bytes, int do_print)
{
	
	if(raw_cmd->reply_count == 7) {
		int end;
		
		if (raw_cmd->reply[3] != raw_cmd->cmd[2]) {
			/* end of cylinder */
			end = raw_cmd->cmd[6] + 1;
		} else {
			end = raw_cmd->reply[5];
		}

		*bytes = end - raw_cmd->cmd[4];
		/* FIXME: over/under run */
		*bytes = *bytes << (7 + raw_cmd->cmd[5]);
	} else
		*bytes = 0;       

	switch(raw_cmd->reply[0] & 0xc0){
		case 0x40:
			if ((raw_cmd->reply[0] & 0x38) == 0 &&
			    (raw_cmd->reply[1]) == 0x80 &&
			    (raw_cmd->reply[2]) == 0) {
				*bytes += 1 << (7 + raw_cmd->cmd[5]);
				break;
			}

			if ( raw_cmd->reply[1] & ST1_WP ){
				*bytes = 0;
				fprintf(stderr,
					"This disk is write protected\n");
				return -1;
			}
			if(!*bytes && do_print)
				print_message(raw_cmd, "");
			return -1;
		case 0x80:
			*bytes = 0;
			fprintf(stderr,
				"invalid command given\n");
			return -1;
		case 0xc0:
			*bytes = 0;
			fprintf(stderr,
				"abnormal termination caused by polling\n");
			return -1;
		default:
			break;
	}	
#ifdef FD_RAW_MORE
	if(raw_cmd->flags & FD_RAW_MORE)
		return 1;
#endif
	return 0;
}

#define predefined_devices
struct device devices[] = {
	{"/dev/fd0", "A", 0, O_EXCL, 80,2, 18,0, MDEF_ARG},
	{"/dev/fd1", "B", 0, O_EXCL, 0,0, 0,0, FDEF_ARG},
	/* we assume that the Zip or Jaz drive is the second on the SCSI bus */
	{"/dev/sdb4","J", GENHD },
	{"/dev/sdb4","Z", GENHD },
	/*	{"/dev/sda4","D", GENHD },*/
	REMOTE
};

/*
 * Stuffing back the floppy parameters into the driver allows for gems
 * like 21 sector or single sided floppies from Atari ST systems.
 * 
 * Alain Knaff, Université Joseph Fourier, France, November 12, 1993.
 */


#define INIT_GENERIC
#define generic_floppy_struct floppy_struct
#define BLOCK_MAJOR 2
#define SECTORS(floppy) floppy.sect
#define TRACKS(floppy) floppy.track
#define HEADS(floppy) floppy.head
#define SECTORS_PER_DISK(floppy) floppy.size
#define STRETCH(floppy) floppy.stretch
#define USE_2M(floppy) ((floppy.rate & FD_2M) ? 0xff : 0x80 )
#define SSIZE(floppy) ((((floppy.rate & 0x38) >> 3 ) + 2) % 8)

static inline void set_2m(struct floppy_struct *floppy, int value)
{
	if (value & 0x7f)
		value = FD_2M;
	else
		value = 0;
	floppy->rate = (floppy->rate & ~FD_2M) | value;       
}
#define SET_2M set_2m

static inline void set_ssize(struct floppy_struct *floppy, int value)
{
	value = (( (value & 7) + 6 ) % 8) << 3;

	floppy->rate = (floppy->rate & ~0x38) | value;	
}

#define SET_SSIZE set_ssize

static inline int set_parameters(int fd, struct floppy_struct *floppy, 
				 struct stat *buf)
{
	if ( ( MINOR(buf->st_rdev ) & 0x7f ) > 3 )
		return 1;
	
	return ioctl(fd, FDSETPRM, floppy);
}

static inline int get_parameters(int fd, struct floppy_struct *floppy)
{
	return ioctl(fd, FDGETPRM, floppy);
}

#endif /* linux */


/* OS/2, gcc+emx */
#ifdef __EMX__
#define predefined_devices
struct device devices[] = {
  {"A:", "A", GENFD},
  {"B:", "B", GENFD},
};
#define INIT_NOOP
#endif



/*** /jes -- for D.O.S. 486 BL DX2/80 ***/
#ifdef OS_freebsd
#define predefined_devices
struct device devices[] = {
	{"/dev/rfd0.1440", "A", FHD312},
	{"/dev/rfd0.720",  "A", FDD312},
	{"/dev/rfd1.1200", "B", MHD514},
	{"/dev/sd0s1",     "C", GENHD},
	REMOTE
};
#endif /* __FreeBSD__ */
 
/*** /jes -- for ALR 486 DX4/100 ***/
#if defined(OS_netbsd)
#define predefined_devices
struct device devices[] = {
	{"/dev/rfd0a", "A", FHD312},
	{"/dev/rfd0f", "A", FDD312},
	{"/dev/rfd0f", "S", MDD312},
	{"/dev/rfd1a", "B", FHD514},
	{"/dev/rfd1d", "B", FDD514},
	{"/dev/rfd1d", "T", MDD514},
	{"/dev/rwd0d", "C", 16, 0, 0, 0, 0, 0, 63L*512L, DEF_ARG0(0)},
	REMOTE
};
#endif /* OS_NetBSD */

/* fgsch@openbsd.org 2000/05/19 */
#if defined(OS_openbsd)
#define predefined_devices
struct device devices[] = {
	{"/dev/rfd0Bc", "A", FHD312},
	{"/dev/rfd0Fc", "A", FDD312},
	{"/dev/rfd1Cc", "B", FHD514},
	{"/dev/rfd1Dc", "B", FDD514},
	{"/dev/rwd0c", "C", 16, 0, 0, 0, 0, 0, 63L*512L, DEF_ARG0(0)},
	REMOTE
};
#endif /* OS_openbsd */



#if (!defined(predefined_devices) && defined (CPU_m68000) && defined (OS_sysv))
#include <sys/gdioctl.h>

#define predefined_devices
struct device devices[] = {
	{"/dev/rfp020",		"A", 12,O_NDELAY,40,2, 9, 0, MDEF_ARG},
	{"/usr/bin/DOS/dvd000", "C", GENFD},
	REMOTE
};

#undef INIT_NOOP
int init_geom(int fd, struct device *dev, struct device *orig_dev,
	      struct stat *stat)
{
	struct gdctl gdbuf;

	if (ioctl(fd, GDGETA, &gdbuf) == -1) {
		ioctl(fd, GDDISMNT, &gdbuf);
		return 1;
	}
	if((dev->use_2m & 0x7f) || (dev->ssize & 0x7f))
		return 1;
	
	SET_INT(gdbuf.params.cyls,dev->ntracks);
	SET_INT(gdbuf.params.heads,dev->nheads);
	SET_INT(gdbuf.params.psectrk,dev->nsect);
	dev->ntracks = gdbuf.params.cyls;
	dev->nheads = gdbuf.params.heads;
	dev->nsect = gdbuf.params.psectrk;
	dev->use_2m = 0x80;
	dev->ssize = 0x82;

	gdbuf.params.pseccyl = gdbuf.params.psectrk * gdbuf.params.heads;
	gdbuf.params.flags = 1;		/* disk type flag */
	gdbuf.params.step = 0;		/* step rate for controller */
	gdbuf.params.sectorsz = 512;	/* sector size */

	if (ioctl(fd, GDSETA, &gdbuf) < 0) {
		ioctl(fd, GDDISMNT, &gdbuf);
		return(1);
	}
	return(0);
}
#endif /* (defined (m68000) && defined (sysv))*/

#ifdef CPU_alpha
#ifndef OS_osf4
#ifdef __osf__
#include <sys/fcntl.h>
#define predefined_devices
struct device devices[] = {
	{"/dev/rfd0c",		"A", GENFD},
	REMOTE
};
#endif
#endif
#endif

#ifdef OS_osf
#ifndef predefined_devices
#define predefined_devices
struct device devices[] = {
	{"/dev/fd0a", "A",  MHD312 } };
	REMOTE
#endif
#endif


#ifdef OS_nextstep
#define predefined_devices
struct device devices[] = {
#ifdef CPU_m68k
	{"/dev/rfd0b", "A", MED312 },
	REMOTE
#else
	{"/dev/rfd0b", "A", MHD312 },
	REMOTE
#endif
};
#endif


#if (!defined(predefined_devices) && defined(OS_sysv4))
#ifdef __uxp__
#define predefined_devices
struct device devices[] = {
      {"/dev/fpd0",   "A", FHD312},
      {"/dev/fpd0",   "A", FDD312},
	  REMOTE
};
#else
#define predefined_devices
struct device devices[] = {
	{"/dev/rdsk/f1q15dt",	"B", FHD514},
	{"/dev/rdsk/f1d9dt",	"B", FDD514},
	{"/dev/rdsk/f1d8dt",	"B", FDDsmall},
	{"/dev/rdsk/f03ht",	"A", FHD312},
	{"/dev/rdsk/f03dt",	"A", FDD312},
	{"/dev/rdsk/dos",	"C", GENHD},
	REMOTE
};
#endif
#endif /* sysv4 */

#ifdef OS_Minix
/* Minix and Minix-vmd device list.  Only present to attach the A: and B:
 * drive letters to the floppies by default.  Other devices can be given
 * a drive letter by linking the device file to /dev/dosX, where X is a
 * drive letter.  Or one can use something like 'fd0:' for a drive name.
 *						Kees J. Bot <kjb@cs.vu.nl>
 */
#include <minix/partition.h>
#include <minix/u64.h>

#define predefined_devices
struct device devices[] = {
	{"/dev/fd0", "A", GEN},
	{"/dev/fd1", "B", GEN},
};

#undef INIT_NOOP
int init_geom(int fd, struct device *dev, struct device *orig_dev,
	      struct stat *stat)
{
	/* Try to obtain the device parameters from the device driver.
	 * Don't fret if you can't, mtools will use the DOS boot block.
	 */
	struct partition geom;
	unsigned long tot_sectors;

	if (ioctl(fd, DIOCGETP, &geom) == 0) {
		dev->hidden = div64u(geom.base, 512);
		tot_sectors = div64u(geom.size, 512);
		dev->tracks = tot_sectors / (geom.heads * geom.sectors);
		dev->heads = geom.heads;
		dev->sectors = geom.sectors;
	}
	return(0);
}
#endif /* OS_Minix */

#ifdef INIT_GENERIC

#ifndef USE_2M
#define USE_2M(x) 0x80
#endif

#ifndef SSIZE
#define SSIZE(x) 0x82
#endif

#ifndef SET_2M
#define SET_2M(x,y) return -1
#endif

#ifndef SET_SSIZE
#define SET_SSIZE(x,y) return -1
#endif

#undef INIT_NOOP
int init_geom(int fd, struct device *dev, struct device *orig_dev,
	      struct stat *stat)
{
	struct generic_floppy_struct floppy;
	int change;
	
	/* 
	 * succeed if we don't have a floppy
	 * this is the case for dosemu floppy image files for instance
	 */
	if (!((S_ISBLK(stat->st_mode) && major(stat->st_rdev) == BLOCK_MAJOR)
#ifdef CHAR_MAJOR
	      || (S_ISCHR(stat->st_mode) && major(stat->st_rdev) == CHAR_MAJOR) 
#endif
		))
		return compare_geom(dev, orig_dev);
	
	/*
	 * We first try to get the current floppy parameters from the kernel.
	 * This allows us to
	 * 1. get the rate
	 * 2. skip the parameter setting if the parameters are already o.k.
	 */
	
	if (get_parameters( fd, & floppy ) )
		/* 
		 * autodetection failure.
		 * This mostly occurs because of an absent or unformatted disks.
		 *
		 * It might also occur because of bizarre formats (for example 
		 * rate 1 on a 3 1/2 disk).

		 * If this is the case, the user should do an explicit 
		 * setfdprm before calling mtools
		 *
		 * Another cause might be pre-existing wrong parameters. The 
		 * user should do an setfdprm -c to repair this situation.
		 *
		 * ...fail immediately... ( Theoretically, we could try to save
		 * the situation by trying out all rates, but it would be slow 
		 * and awkward)
		 */
		return 1;


	/* 
	 * if we have already have the correct parameters, keep them.
	 * the number of tracks doesn't need to match exactly, it may be bigger.
	 * the number of heads and sectors must match exactly, to avoid 
	 * miscalculation of the location of a block on the disk
	 */
	change = 0;
	if(compare(dev->sectors, SECTORS(floppy))){
		SECTORS(floppy) = dev->sectors;
		change = 1;
	} else
		dev->sectors = SECTORS(floppy);

	if(compare(dev->heads, HEADS(floppy))){
		HEADS(floppy) = dev->heads;
		change = 1;
	} else
		dev->heads = HEADS(floppy);
	 
	if(compare(dev->tracks, TRACKS(floppy))){
		TRACKS(floppy) = dev->tracks;
		change = 1;
	} else
		dev->tracks = TRACKS(floppy);


	if(compare(dev->use_2m, USE_2M(floppy))){
		SET_2M(&floppy, dev->use_2m);
		change = 1;
	} else
		dev->use_2m = USE_2M(floppy);
	
	if( ! (dev->ssize & 0x80) )
		dev->ssize = 0;
	if(compare(dev->ssize, SSIZE(floppy) + 128)){
		SET_SSIZE(&floppy, dev->ssize);
		change = 1;
	} else
		dev->ssize = SSIZE(floppy);

	if(!change)
		/* no change, succeed */
		return 0;

#ifdef SECTORS_PER_TRACK
	SECTORS_PER_TRACK(floppy) = dev->sectors * dev->heads;
#endif

#ifdef SECTORS_PER_DISK
	SECTORS_PER_DISK(floppy) = dev->sectors * dev->heads * dev->tracks;
#endif
	
#ifdef STRETCH
	/* ... and the stretch */
	if ( dev->tracks > 41 ) 
		STRETCH(floppy) = 0;
	else
		STRETCH(floppy) = 1;
#endif
	
	return set_parameters( fd, &floppy, stat) ;
}
#endif /* INIT_GENERIC */  

#ifdef INIT_NOOP
int init_geom(int fd, struct device *dev, struct device *orig_dev,
			  struct stat *stat)
{
	return compare_geom(dev, orig_dev);
}
#endif

#ifdef predefined_devices
const int nr_const_devices = sizeof(const_devices) / sizeof(*const_devices);
#else
struct device devices[]={
	{"/dev/fd0", "A", 0, O_EXCL, 0,0, 0,0, MDEF_ARG},
	/* to shut up Ultrix's native compiler, we can't make this empty :( */
};
const nr_const_devices = 0;
#endif

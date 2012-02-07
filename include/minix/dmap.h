#ifndef _DMAP_H
#define _DMAP_H

#include <minix/sys_config.h>
#include <minix/ipc.h>

enum dev_style { STYLE_NDEV, STYLE_DEV, STYLE_DEVA, STYLE_TTY, STYLE_CTTY,
	STYLE_CLONE, STYLE_CLONE_A };
#define IS_DEV_STYLE(s) (s>=STYLE_NDEV && s<=STYLE_CLONE_A)

#define dev_style_asyn(devstyle)	((devstyle) == STYLE_DEVA || \
					(devstyle) == STYLE_CLONE_A)

/*===========================================================================*
 *               	 Major and minor device numbers  		     *
 *===========================================================================*/

/* Total number of different devices. */
#define NR_DEVICES   	NR_SYS_PROCS	/* number of (major) devices */

/* Major device numbers. */
#define NONE_MAJOR		   0	/*  0 = not used                      */
#define MEMORY_MAJOR  		   1	/*  1 = /dev/mem    (memory devices)  */
#define FLOPPY_MAJOR	           2	/*  2 = /dev/fd0    (floppy disks)    */
                                        /*  3 = /dev/c0                       */
#define TTY_MAJOR		   4	/*  4 = /dev/tty00  (ttys)            */
#define CTTY_MAJOR		   5	/*  5 = /dev/tty                      */
#define PRINTER_MAJOR		   6	/*  6 = /dev/lp     (printer driver)  */
#define INET_MAJOR		   7	/*  7 = /dev/ip     (inet)            */
					/*  8 = /dev/c1                       */
					/*  9 = not used                      */
					/* 10 = /dev/c2                       */
#define FILTER_MAJOR		  11	/* 11 = /dev/filter (filter driver)   */
					/* 12 = /dev/c3                       */
#define AUDIO_MAJOR		  13	/* 13 = /dev/audio  (audio driver)    */
#define FBD_MAJOR		  14	/* 14 = /dev/fbd    (faulty block dev)*/
#define LOG_MAJOR		  15	/* 15 = /dev/klog   (log driver)      */
#define RANDOM_MAJOR		  16	/* 16 = /dev/random (random driver)   */
#define HELLO_MAJOR		  17	/* 17 = /dev/hello  (hello driver)    */
#define UDS_MAJOR		  18	/* 18 = /dev/uds    (pfs)             */


/* Minor device numbers for memory driver. */
#  define RAM_DEV_OLD  		   0	/* minor device for /dev/ram */
#  define MEM_DEV     		   1	/* minor device for /dev/mem */
#  define KMEM_DEV    		   2	/* minor device for /dev/kmem */
#  define NULL_DEV    		   3	/* minor device for /dev/null */
#  define BOOT_DEV    		   4	/* minor device for /dev/boot */
#  define ZERO_DEV    		   5	/* minor device for /dev/zero */
#  define IMGRD_DEV   		   6	/* minor device for /dev/imgrd */
#  define RAM_DEV_FIRST		   7	/* first minor device for /dev/ram* */

#define CTRLR(n) ((n)==0 ? 3 : (8 + 2*((n)-1)))	/* magic formula */

/* Minor device numbers for log driver. */
#  define IS_KLOG_DEV		   0	/* minor device for /dev/klog */

/* Full device numbers that are special to the boot monitor and FS. */
#  define DEV_RAM     ((dev_t) 0x0100)	/* device number of /dev/ram */
#  define DEV_IMGRD   ((dev_t) 0x0106)	/* device number of /dev/imgrd */

#endif /* _DMAP_H */


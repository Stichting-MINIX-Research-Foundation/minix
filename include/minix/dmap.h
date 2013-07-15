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
#define FB_MAJOR		  19	/* 18 = /dev/fb0    (fb driver)       */
#define I2C0_MAJOR		  20	/* 20 = /dev/i2c-1  (i2c-dev)         */
#define I2C1_MAJOR		  21	/* 21 = /dev/i2c-2  (i2c-dev)         */
#define I2C2_MAJOR		  22	/* 22 = /dev/i2c-3  (i2c-dev)         */
#define EEPROMB1S50_MAJOR	  23	/* 23 = /dev/eepromb1s50 (cat24c256)  */
#define EEPROMB1S51_MAJOR	  24	/* 24 = /dev/eepromb1s51 (cat24c256)  */
#define EEPROMB1S52_MAJOR	  25	/* 25 = /dev/eepromb1s52 (cat24c256)  */
#define EEPROMB1S53_MAJOR	  26	/* 26 = /dev/eepromb1s53 (cat24c256)  */
#define EEPROMB1S54_MAJOR	  27	/* 27 = /dev/eepromb1s54 (cat24c256)  */
#define EEPROMB1S55_MAJOR	  28	/* 28 = /dev/eepromb1s55 (cat24c256)  */
#define EEPROMB1S56_MAJOR	  29	/* 29 = /dev/eepromb1s56 (cat24c256)  */
#define EEPROMB1S57_MAJOR	  30	/* 30 = /dev/eepromb1s57 (cat24c256)  */
#define EEPROMB2S50_MAJOR	  31	/* 31 = /dev/eepromb2s50 (cat24c256)  */
#define EEPROMB2S51_MAJOR	  32	/* 32 = /dev/eepromb2s51 (cat24c256)  */
#define EEPROMB2S52_MAJOR	  33	/* 33 = /dev/eepromb2s52 (cat24c256)  */
#define EEPROMB2S53_MAJOR	  34	/* 34 = /dev/eepromb2s53 (cat24c256)  */
#define EEPROMB2S54_MAJOR	  35	/* 35 = /dev/eepromb2s54 (cat24c256)  */
#define EEPROMB2S55_MAJOR	  36	/* 36 = /dev/eepromb2s55 (cat24c256)  */
#define EEPROMB2S56_MAJOR	  37	/* 37 = /dev/eepromb2s56 (cat24c256)  */
#define EEPROMB2S57_MAJOR	  38	/* 38 = /dev/eepromb2s57 (cat24c256)  */
#define EEPROMB3S50_MAJOR	  39	/* 39 = /dev/eepromb3s50 (cat24c256)  */
#define EEPROMB3S51_MAJOR	  40	/* 40 = /dev/eepromb3s51 (cat24c256)  */
#define EEPROMB3S52_MAJOR	  41	/* 41 = /dev/eepromb3s52 (cat24c256)  */
#define EEPROMB3S53_MAJOR	  42	/* 42 = /dev/eepromb3s53 (cat24c256)  */
#define EEPROMB3S54_MAJOR	  43	/* 43 = /dev/eepromb3s54 (cat24c256)  */
#define EEPROMB3S55_MAJOR	  44	/* 44 = /dev/eepromb3s55 (cat24c256)  */
#define EEPROMB3S56_MAJOR	  45	/* 45 = /dev/eepromb3s56 (cat24c256)  */
#define EEPROMB3S57_MAJOR	  46	/* 46 = /dev/eepromb3s57 (cat24c256)  */

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


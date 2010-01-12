#ifndef _DMAP_H
#define _DMAP_H

#include <minix/sys_config.h>
#include <minix/ipc.h>

enum dev_style { STYLE_DEV, STYLE_NDEV, STYLE_TTY, STYLE_CLONE };

/*===========================================================================*
 *               	 Major and minor device numbers  		     *
 *===========================================================================*/

/* Total number of different devices. */
#define NR_DEVICES   		  32		/* number of (major) devices */

#define NONE_MAJOR		   0	/* pseudo device for mounting file
					 * systems without a real block device
					 */

/* Major and minor device numbers for MEMORY driver. */
#define MEMORY_MAJOR  		   1	/* major device for memory devices */
#  define RAM_DEV_OLD  		   0	/* minor device for /dev/ram */
#  define MEM_DEV     		   1	/* minor device for /dev/mem */
#  define KMEM_DEV    		   2	/* minor device for /dev/kmem */
#  define NULL_DEV    		   3	/* minor device for /dev/null */
#  define BOOT_DEV    		   4	/* minor device for /dev/boot */
#  define ZERO_DEV    		   5	/* minor device for /dev/zero */
#  define IMGRD_DEV   		   6	/* minor device for /dev/imgrd */
#  define RAM_DEV_FIRST		   7	/* first minor device for /dev/ram* */

#define CTRLR(n) ((n)==0 ? 3 : (8 + 2*((n)-1)))	/* magic formula */

/* Full device numbers that are special to the boot monitor and FS. */
#  define DEV_RAM	      0x0100	/* device number of /dev/ram */
#  define DEV_IMGRD	      0x0106	/* device number of /dev/imgrd */

#define FLOPPY_MAJOR	           2	/* major device for floppy disks */
#define TTY_MAJOR		   4	/* major device for ttys */
#define CTTY_MAJOR		   5	/* major device for /dev/tty */

#define INET_MAJOR		   7	/* major device for inet */

#define FILTER_MAJOR		  11	/* major device for filter driver */

#define LOG_MAJOR		  15	/* major device for log driver */
#  define IS_KLOG_DEV		   0	/* minor device for /dev/klog */

#endif /* _DMAP_H */

#ifndef _DMAP_H
#define _DMAP_H

#include <minix/sys_config.h>
#include <minix/ipc.h>

/*===========================================================================*
 *               	 Device <-> Driver Table  			     *
 *===========================================================================*/

/* Device table.  This table is indexed by major device number.  It provides
 * the link between major device numbers and the routines that process them.
 * The table can be update dynamically. The field 'dmap_flags' describe an 
 * entry's current status and determines what control options are possible. 
 */
#define DMAP_MUTABLE		0x01	/* mapping can be overtaken */
#define DMAP_BUSY		0x02	/* driver busy with request */

enum dev_style { STYLE_DEV, STYLE_NDEV, STYLE_TTY, STYLE_CLONE };

extern struct dmap {
  int _PROTOTYPE ((*dmap_opcl), (int, Dev_t, int, int) );
  void _PROTOTYPE ((*dmap_io), (int, message *) );
  int dmap_driver;
  int dmap_flags;
} dmap[];

/*===========================================================================*
 *               	 Major and minor device numbers  		     *
 *===========================================================================*/

/* Total number of different devices. */
#define NR_DEVICES   		  32			/* number of (major) devices */

/* Major and minor device numbers for MEMORY driver. */
#define MEMORY_MAJOR  		   1	/* major device for memory devices */
#  define RAM_DEV     		   0	/* minor device for /dev/ram */
#  define MEM_DEV     		   1	/* minor device for /dev/mem */
#  define KMEM_DEV    		   2	/* minor device for /dev/kmem */
#  define NULL_DEV    		   3	/* minor device for /dev/null */
#  define BOOT_DEV    		   4	/* minor device for /dev/boot */
#  define ZERO_DEV    		   5	/* minor device for /dev/zero */

#define CTRLR(n) ((n)==0 ? 3 : (8 + 2*((n)-1)))	/* magic formula */

/* Full device numbers that are special to the boot monitor and FS. */
#  define DEV_RAM	      0x0100	/* device number of /dev/ram */
#  define DEV_BOOT	      0x0104	/* device number of /dev/boot */

#define FLOPPY_MAJOR	           2	/* major device for floppy disks */
#define TTY_MAJOR		   4	/* major device for ttys */
#define CTTY_MAJOR		   5	/* major device for /dev/tty */

#define INET_MAJOR		   7	/* major device for inet */

#define LOG_MAJOR		  15	/* major device for log driver */
#  define IS_KLOG_DEV		   0	/* minor device for /dev/klog */

#endif /* _DMAP_H */

/* This file contains the rescue device driver (/dev/rescue)
 *
 *  Changes:
 *	Oct 21, 1992	created  (Jorrit N. Herder)
 */

#include "../drivers.h"
#include "../libdriver/driver.h"
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"

#define VERBOSE	 	   0		/* enable/ disable messages */
#define NR_DEVS            1		/* number of rescue devices */
#define RESCUE_KBYTES	 128		/* default size in kilobytes */

PRIVATE struct device m_geom[NR_DEVS];  /* base and size of each device */
PRIVATE int m_seg[NR_DEVS];  		/* segment index of each device */
PRIVATE int m_device;			/* current device */

extern int errno;			/* error number for PM calls */

FORWARD _PROTOTYPE( void m_init, (int argc, char **argv) );
FORWARD _PROTOTYPE( char *m_name, (void) 				);
FORWARD _PROTOTYPE( struct device *m_prepare, (int device) 		);
FORWARD _PROTOTYPE( int m_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) 	);
FORWARD _PROTOTYPE( int m_do_open, (struct driver *dp, message *m_ptr) 	);
FORWARD _PROTOTYPE( void m_geometry, (struct partition *entry) 		);

/* Entry points to this driver. */
PRIVATE struct driver m_dtab = {
  m_name,	/* current device's name */
  m_do_open,	/* open or mount */
  do_nop,	/* nothing on a close */
  do_diocntl,	/* standard I/O controls */
  m_prepare,	/* prepare for I/O on a given minor device */
  m_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  m_geometry,	/* memory device "geometry" */
  nop_signal,	/* system signals */
  nop_alarm,
  nop_cancel,
  nop_select,
  NULL,
  NULL
};


/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
PUBLIC int main(int argc, char **argv)
{
/* Main program. Initialize the rescue driver and start the main loop. */
  m_init(argc, argv);			
  driver_task(&m_dtab);		
  return(OK);				
}

/*===========================================================================*
 *				 m_name					     *
 *===========================================================================*/
PRIVATE char *m_name()
{
/* Return a name for the current device. */
  static char name[] = "rescue";
  return name;  
}

/*===========================================================================*
 *				m_prepare				     *
 *===========================================================================*/
PRIVATE struct device *m_prepare(device)
int device;
{
/* Prepare for I/O on a device: check if the minor device number is ok. */
  if (device < 0 || device >= NR_DEVS) return(NIL_DEV);
  m_device = device;

  return(&m_geom[device]);
}

/*===========================================================================*
 *				m_transfer				     *
 *===========================================================================*/
PRIVATE int m_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
/* Read or write one the driver's minor devices. */
  int seg;
  unsigned count, left, chunk;
  vir_bytes user_vir;
  struct device *dv;
  unsigned long dv_size;
  int s;

  /* Get and check minor device number. */
  if ((unsigned) m_device > NR_DEVS - 1) return(ENXIO);
  dv = &m_geom[m_device];
  dv_size = cv64ul(dv->dv_size);

  while (nr_req > 0) {

	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	user_vir = iov->iov_addr;

	/* Virtual copying. For rescue device. */
	if (position >= dv_size) return(OK); 	/* check for EOF */
	if (position + count > dv_size) count = dv_size - position;
	seg = m_seg[m_device];

	if (opcode == DEV_GATHER) {			/* copy actual data */
	    sys_vircopy(SELF,seg,position, proc_nr,D,user_vir, count);
	} else {
	    sys_vircopy(proc_nr,D,user_vir, SELF,seg,position, count);
	}

	/* Book the number of bytes transferred. */
	position += count;
	iov->iov_addr += count;
  	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; }

  }
  return(OK);
}

/*===========================================================================*
 *				m_do_open				     *
 *===========================================================================*/
PRIVATE int m_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Check device number on open. */
  if (m_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);
  return(OK);
}

/*===========================================================================*
 *				m_init					     *
 *===========================================================================*/
PRIVATE void m_init(argc,argv)
int argc;
char **argv;
{
  /* Initialize this task. All minor devices are initialized one by one. */
  phys_bytes rescue_size;
  phys_bytes rescue_base;
  message m;
  int i, s;

  /* Initialize all rescue devices in a loop. */
  for (i=0; i< NR_DEVS; i++) {

      /* Determine size and base of rescue disks. See if rescue disk details 
       * exist in the data store. If no memory for the rescue disk was claimed 
       * yet, do it below. 
       */
      m.DS_KEY = (RESCUE_MAJOR << 8) + i;
      if (OK == (s = _taskcall(DS_PROC_NR, DS_RETRIEVE, &m))) {
          rescue_size = m.DS_VAL_L1;
          rescue_base = m.DS_VAL_L2;
      }
      else {					/* no details known */
          if (argc>i+1) rescue_size = atoi(argv[i+1]) * 1024;
          else 		rescue_size = RESCUE_KBYTES * 1024;

          if (allocmem(rescue_size, &rescue_base) < 0) {
              report("RESCUE", "warning, allocmem failed", errno);
              rescue_size = 0;
          }
      }

      /* Now that we have the base and size of the rescue disk, set up all
       * data structures if the rescue has a positive (nonzero) size. 
       */
      if (rescue_size > 0) {

          /* Create a new remote segment to make virtual copies. */ 
          if (OK != (s=sys_segctl(&m_seg[i], (u16_t *) &s, 
                  (vir_bytes *) &s, rescue_base, rescue_size))) {
              panic("RESCUE","Couldn't install remote segment.",s);
          }

          /* Set the device geometry for the outside world. */
          m_geom[i].dv_base = cvul64(rescue_base);
          m_geom[i].dv_size = cvul64(rescue_size);

          /* Store the values in the data store for future retrieval. */
          m.DS_KEY = (RESCUE_MAJOR << 8) + i;
          m.DS_VAL_L1 = rescue_size;
          m.DS_VAL_L2 = rescue_base;
          if (OK != (s = _taskcall(DS_PROC_NR, DS_PUBLISH, &m))) {
              panic("RESCUE","Couldn't store rescue disk details at DS.",s);
          }

#if VERBOSE
          printf("RESCUE disk %d (size %u/base %u) initialized\n",
              i, rescue_size, rescue_base);
#endif
      }
  }
}

/*===========================================================================*
 *				m_geometry				     *
 *===========================================================================*/
PRIVATE void m_geometry(entry)
struct partition *entry;
{
  /* Memory devices don't have a geometry, but the outside world insists. */
  entry->cylinders = div64u(m_geom[m_device].dv_size, SECTOR_SIZE) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}


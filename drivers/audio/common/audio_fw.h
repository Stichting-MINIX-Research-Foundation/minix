#ifndef AUDIO_FW_H
#define AUDIO_FW_H

#include <minix/drivers.h>
#include <minix/driver.h>
#include <sys/ioc_sound.h>


/* change to DEBUG to 1 to print debug info and error messages */

#define DEBUG 0

#if DEBUG
#define dprint printf 
#else
#define dprint (void) 
#endif
#define error printf


_PROTOTYPE( int drv_init, (void) );
_PROTOTYPE( int drv_init_hw, (void) );
_PROTOTYPE( int drv_reset, (void) );
_PROTOTYPE( int drv_start, (int sub_dev, int DmaMode) );
_PROTOTYPE( int drv_stop, (int sub_dev) );
_PROTOTYPE( int drv_set_dma, (u32_t dma, u32_t length, int chan) );
_PROTOTYPE( int drv_reenable_int, (int chan) );
_PROTOTYPE( int drv_int_sum, (void) );
_PROTOTYPE( int drv_int, (int sub_dev) );
_PROTOTYPE( int drv_pause, (int chan) );
_PROTOTYPE( int drv_resume, (int chan) );
_PROTOTYPE( int drv_io_ctl, (int request, void * val, int * len, int sub_dev) );
_PROTOTYPE( int drv_get_irq, (char *irq) );
_PROTOTYPE( int drv_get_frag_size, (u32_t *frag_size, int sub_dev) );



/* runtime status fields */
typedef struct {
	int readable;
	int writable;
	int DmaSize;
	int NrOfDmaFragments;
	int MinFragmentSize;
	int NrOfExtraBuffers;
	int Nr;                                   /* sub device number */
	int Opened;                               /* sub device opened */
	int DmaBusy;                              /* is dma busy? */
	int DmaMode;                              /* DEV_WRITE / DEV_READ */
	int DmaReadNext;                          /* current dma buffer */
	int DmaFillNext;                          /* next dma buffer to fill */
	int DmaLength;
	int BufReadNext;                          /* start of extra circular buffer */
	int BufFillNext;                          /* end of extra circular buffer */
	int BufLength;
	int RevivePending;                        /* process waiting for this dev? */
	int ReviveStatus;                         /* return val when proc unblocked */
	int ReviveProcNr;                         /* the process to unblock */
	cp_grant_id_t ReviveGrant;		  /* grant id associated with io */
	void *UserBuf;                            /* address of user's data buffer */
	int ReadyToRevive;                        /* are we ready to revive process?*/
	int NotifyProcNr;                         /* process to send notify to (FS) */
	u32_t FragSize;                           /* dma fragment size */
	char *DmaBuf;        /* the dma buffer; extra space for 
												  page alignment */
	phys_bytes DmaPhys;                       /* physical address of dma buffer */
	char* DmaPtr;                             /* pointer to aligned dma buffer */
	int OutOfData;                            /* all buffers empty? */
	char *ExtraBuf;                           /* don't use extra buffer;just 
											   declare a pointer to supress
											   error messages */
} sub_dev_t;

typedef struct {
	int minor_dev_nr;
	int read_chan;
	int write_chan;
	int io_ctl;
} special_file_t;

typedef struct {
	char* DriverName;
	int NrOfSubDevices;
	int NrOfSpecialFiles;
} drv_t;

EXTERN drv_t drv;
EXTERN sub_dev_t sub_dev[];
EXTERN special_file_t special_file[];

/* Number of bytes you can DMA before hitting a 64K boundary: */
#define dma_bytes_left(phys)    \
   ((unsigned) (sizeof(int) == 2 ? 0 : 0x10000) - (unsigned) ((phys) & 0xFFFF))

#define NO_CHANNEL -1

#define TRUE 1
#define FALSE 0
#define NO_DMA 0


#endif /* AUDIO_FW_H */

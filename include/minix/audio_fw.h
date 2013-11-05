#ifndef AUDIO_FW_H
#define AUDIO_FW_H

#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <sys/ioc_sound.h>


int drv_init(void);
int drv_init_hw(void);
int drv_reset(void);
int drv_start(int sub_dev, int DmaMode);
int drv_stop(int sub_dev);
int drv_set_dma(u32_t dma, u32_t length, int chan);
int drv_reenable_int(int chan);
int drv_int_sum(void);
int drv_int(int sub_dev);
int drv_pause(int chan);
int drv_resume(int chan);
int drv_io_ctl(unsigned long request, void * val, int * len, int sub_dev);
int drv_get_irq(char *irq);
int drv_get_frag_size(u32_t *frag_size, int sub_dev);



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
	endpoint_t ReviveProcNr;                  /* the process to unblock */
	cdev_id_t ReviveId;                       /* request ID */
	cp_grant_id_t ReviveGrant;		  /* grant id associated with io */
	endpoint_t SourceProcNr;                  /* process to send notify to (FS) */
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

#define NO_DMA 0
#define READ_DMA 1
#define WRITE_DMA 2

#endif /* AUDIO_FW_H */

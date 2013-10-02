#include <minix/drivers.h>
#include <minix/blockdriver.h>
#include <minix/drvlib.h>

#define VERBOSE		   0	/* display identify messages during boot */
#define VERBOSE_DMA	   0	/* display DMA debugging information */

#define ATAPI_DEBUG	    0	/* To debug ATAPI code. */

/* I/O Ports used by winchester disk controllers. */

/* Read and write registers */
#define REG_CMD_BASE0	0x1F0	/* command base register of controller 0 */
#define REG_CMD_BASE1	0x170	/* command base register of controller 1 */
#define REG_CTL_BASE0	0x3F6	/* control base register of controller 0 */
#define REG_CTL_BASE1	0x376	/* control base register of controller 1 */

#define PCI_CTL_OFF	    2	/* Offset of control registers from BAR2 */
#define PCI_DMA_2ND_OFF	    8	/* Offset of DMA registers from BAR4 for 
				 * secondary channel
				 */

#define REG_DATA	    0	/* data register (offset from the base reg.) */
#define REG_PRECOMP	    1	/* start of write precompensation */
#define REG_COUNT	    2	/* sectors to transfer */
#define REG_SECTOR	    3	/* sector number */
#define REG_CYL_LO	    4	/* low byte of cylinder number */
#define REG_CYL_HI	    5	/* high byte of cylinder number */
#define REG_LDH		    6	/* lba, drive and head */
#define   LDH_DEFAULT		0xA0	/* ECC enable, 512 bytes per sector */
#define   LDH_LBA		0x40	/* Use LBA addressing */
#define	  LDH_DEV		0x10	/* Drive 1 iff set */
#define   ldh_init(drive)	(LDH_DEFAULT | ((drive) << 4))

/* Read only registers */
#define REG_STATUS	    7	/* status */
#define   STATUS_BSY		0x80	/* controller busy */
#define	  STATUS_RDY		0x40	/* drive ready */
#define	  STATUS_WF		0x20	/* write fault */
#define	  STATUS_SC		0x10	/* seek complete (obsolete) */
#define	  STATUS_DRQ		0x08	/* data transfer request */
#define	  STATUS_CRD		0x04	/* corrected data */
#define	  STATUS_IDX		0x02	/* index pulse */
#define	  STATUS_ERR		0x01	/* error */
#define	  STATUS_ADMBSY	       0x100	/* administratively busy (software) */
#define REG_ERROR	    1	/* error code */
#define	  ERROR_BB		0x80	/* bad block */
#define	  ERROR_ECC		0x40	/* bad ecc bytes */
#define	  ERROR_ID		0x10	/* id not found */
#define	  ERROR_AC		0x04	/* aborted command */
#define	  ERROR_TK		0x02	/* track zero error */
#define	  ERROR_DM		0x01	/* no data address mark */

/* Write only registers */
#define REG_COMMAND	    7	/* command */
#define   CMD_IDLE		0x00	/* for w_command: drive idle */
#define   CMD_RECALIBRATE	0x10	/* recalibrate drive */
#define   CMD_READ		0x20	/* read data */
#define   CMD_READ_EXT		0x24	/* read data (LBA48 addressed) */
#define   CMD_READ_DMA_EXT	0x25	/* read data using DMA (w/ LBA48) */
#define   CMD_WRITE		0x30	/* write data */
#define	  CMD_WRITE_EXT		0x34	/* write data (LBA48 addressed) */
#define   CMD_WRITE_DMA_EXT	0x35	/* write data using DMA (w/ LBA48) */
#define   CMD_READVERIFY	0x40	/* read verify */
#define   CMD_FORMAT		0x50	/* format track */
#define   CMD_SEEK		0x70	/* seek cylinder */
#define   CMD_DIAG		0x90	/* execute device diagnostics */
#define   CMD_SPECIFY		0x91	/* specify parameters */
#define   CMD_READ_DMA		0xC8	/* read data using DMA */
#define   CMD_WRITE_DMA		0xCA	/* write data using DMA */
#define   CMD_FLUSH_CACHE	0xE7	/* flush the write cache */
#define   ATA_IDENTIFY		0xEC	/* identify drive */
/* #define REG_CTL		0x206	*/ /* control register */
#define REG_CTL		0	/* control register */
#define   CTL_NORETRY		0x80	/* disable access retry */
#define   CTL_NOECC		0x40	/* disable ecc retry */
#define   CTL_EIGHTHEADS	0x08	/* more than eight heads */
#define   CTL_RESET		0x04	/* reset controller */
#define   CTL_INTDISABLE	0x02	/* disable interrupts */
#define REG_CTL_ALTSTAT 0	/* alternate status register */

/* Identify words */
#define ID_GENERAL		0x00	/* General configuration information */
#define		ID_GEN_NOT_ATA		0x8000	/* Not an ATA device */
#define ID_CAPABILITIES		0x31	/* Capabilities (49)*/
#define		ID_CAP_LBA		0x0200	/* LBA supported */
#define		ID_CAP_DMA		0x0100	/* DMA supported */
#define ID_FIELD_VALIDITY	0x35	/* Field Validity (53) */
#define		ID_FV_88		0x04	/* Word 88 is valid (UDMA) */
#define ID_MULTIWORD_DMA	0x3f	/* Multiword DMA (63) */
#define		ID_MWDMA_2_SEL		0x0400	/* Mode 2 is selected */
#define		ID_MWDMA_1_SEL		0x0200	/* Mode 1 is selected */
#define		ID_MWDMA_0_SEL		0x0100	/* Mode 0 is selected */
#define		ID_MWDMA_2_SUP		0x0004	/* Mode 2 is supported */
#define		ID_MWDMA_1_SUP		0x0002	/* Mode 1 is supported */
#define		ID_MWDMA_0_SUP		0x0001	/* Mode 0 is supported */
#define ID_CSS			0x53	/* Command Sets Supported (83) */
#define		ID_CSS_LBA48		0x0400
#define ID_ULTRA_DMA		0x58	/* Ultra DMA (88) */
#define		ID_UDMA_5_SEL		0x2000	/* Mode 5 is selected */
#define		ID_UDMA_4_SEL		0x1000	/* Mode 4 is selected */
#define		ID_UDMA_3_SEL		0x0800	/* Mode 3 is selected */
#define		ID_UDMA_2_SEL		0x0400	/* Mode 2 is selected */
#define		ID_UDMA_1_SEL		0x0200	/* Mode 1 is selected */
#define		ID_UDMA_0_SEL		0x0100	/* Mode 0 is selected */
#define		ID_UDMA_5_SUP		0x0020	/* Mode 5 is supported */
#define		ID_UDMA_4_SUP		0x0010	/* Mode 4 is supported */
#define		ID_UDMA_3_SUP		0x0008	/* Mode 3 is supported */
#define		ID_UDMA_2_SUP		0x0004	/* Mode 2 is supported */
#define		ID_UDMA_1_SUP		0x0002	/* Mode 1 is supported */
#define		ID_UDMA_0_SUP		0x0001	/* Mode 0 is supported */

/* DMA registers */
#define DMA_COMMAND		0		/* Command register */
#define		DMA_CMD_WRITE		0x08	/* PCI bus master writes */
#define		DMA_CMD_START		0x01	/* Start Bus Master */
#define DMA_STATUS		2		/* Status register */
#define		DMA_ST_D1_DMACAP	0x40	/* Drive 1 is DMA capable */
#define		DMA_ST_D0_DMACAP	0x20	/* Drive 0 is DMA capable */
#define		DMA_ST_INT		0x04	/* Interrupt */
#define		DMA_ST_ERROR		0x02	/* Error */
#define		DMA_ST_BM_ACTIVE	0x01	/* Bus Master IDE Active */
#define DMA_PRDTP		4		/* PRD Table Pointer */

/* Check for the presence of LBA48 only on drives that are 'big'. */
#define LBA48_CHECK_SIZE	0x0f000000
#define LBA_MAX_SIZE		0x0fffffff	/* Highest sector size for
						 * regular LBA.
						 */

#define   ERROR_SENSE           0xF0    /* sense key mask */
#define     SENSE_NONE          0x00    /* no sense key */
#define     SENSE_RECERR        0x10    /* recovered error */
#define     SENSE_NOTRDY        0x20    /* not ready */
#define     SENSE_MEDERR        0x30    /* medium error */
#define     SENSE_HRDERR        0x40    /* hardware error */
#define     SENSE_ILRQST        0x50    /* illegal request */
#define     SENSE_UATTN         0x60    /* unit attention */
#define     SENSE_DPROT         0x70    /* data protect */
#define     SENSE_ABRT          0xb0    /* aborted command */
#define     SENSE_MISCOM        0xe0    /* miscompare */
#define   ERROR_MCR             0x08    /* media change requested */
#define   ERROR_ABRT            0x04    /* aborted command */
#define   ERROR_EOM             0x02    /* end of media detected */
#define   ERROR_ILI             0x01    /* illegal length indication */
#define REG_FEAT            1   /* features */
#define   FEAT_OVERLAP          0x02    /* overlap */
#define   FEAT_DMA              0x01    /* dma */
#define REG_IRR             2   /* interrupt reason register */
#define   IRR_REL               0x04    /* release */
#define   IRR_IO                0x02    /* direction for xfer */
#define   IRR_COD               0x01    /* command or data */
#define REG_SAMTAG          3
#define REG_CNT_LO          4   /* low byte of cylinder number */
#define REG_CNT_HI          5   /* high byte of cylinder number */
#define REG_DRIVE           6   /* drive select */

#define REG_STATUS          7   /* status */
#define   STATUS_BSY            0x80    /* controller busy */
#define   STATUS_DRDY           0x40    /* drive ready */
#define   STATUS_DMADF          0x20    /* dma ready/drive fault */
#define   STATUS_SRVCDSC        0x10    /* service or dsc */
#define   STATUS_DRQ            0x08    /* data transfer request */
#define   STATUS_CORR           0x04    /* correctable error occurred */
#define   STATUS_CHECK          0x01    /* check error */

#define   ATAPI_PACKETCMD       0xA0    /* packet command */
#define   ATAPI_IDENTIFY        0xA1    /* identify drive */
#define   SCSI_READ10           0x28    /* read from disk */
#define   SCSI_SENSE            0x03    /* sense request */

#define ATAPI_PACKETSIZE	12
#define SENSE_PACKETSIZE	18

/* Error codes */
#define ERR		 (-1)	/* general error */
#define ERR_BAD_SECTOR	 (-2)	/* block marked bad detected */

/* Some controllers don't interrupt, the clock will wake us up. */
#define WAKEUP_SECS	32	/* drive may be out for 31 seconds max */
#define WAKEUP_TICKS	(WAKEUP_SECS*system_hz)

/* Miscellaneous. */
#define MAX_DRIVES         4	/* max number of actual drives per instance */
#define MAX_DRIVENODES     8	/* number of drive nodes, for node numbering */
#define MAX_SECS	 256	/* controller can transfer this many sectors */
#define MAX_ERRORS         4	/* how often to try rd/wt before quitting */
#define NR_MINORS       (MAX_DRIVENODES * DEV_PER_DRIVE)
#define NR_SUBDEVS	(MAX_DRIVENODES * SUB_PER_DRIVE)
#define DELAY_USECS     1000	/* controller timeout in microseconds */
#define DELAY_TICKS 	   1	/* controller timeout in ticks */
#define DEF_TIMEOUT_USECS 5000000L  /* controller timeout in microseconds */
#define RECOVERY_USECS 500000	/* controller recovery time in microseconds */
#define RECOVERY_TICKS    30	/* controller recovery time in ticks */
#define INITIALIZED	0x01	/* drive is initialized */
#define DEAF		0x02	/* controller must be reset */
#define SMART		0x04	/* drive supports ATA commands */
#define ATAPI		0x08	/* it is an ATAPI device */
#define IDENTIFIED	0x10	/* w_identify done successfully */
#define IGNORING	0x20	/* w_identify failed once */

#define NO_DMA_VAR 	"ata_no_dma"

#define ATA_IF_NATIVE0	(1L << 0)	/* first channel is in native mode */
#define ATA_IF_NATIVE1	(1L << 2)	/* second channel is in native mode */

extern int sef_cb_lu_prepare(int state);
extern int sef_cb_lu_state_isvalid(int state);
extern void sef_cb_lu_state_dump(int state);

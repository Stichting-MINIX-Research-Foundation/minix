/*	$NetBSD: wdsreg.h,v 1.8 2005/12/11 12:22:03 christos Exp $	*/

typedef u_char physaddr[3];
typedef u_char physlen[3];
#define	ltophys	_lto3b
#define	phystol	_3btol

/* WD7000 registers */
#define WDS_STAT		0	/* read */
#define WDS_IRQSTAT		1	/* read */

#define WDS_CMD			0	/* write */
#define WDS_IRQACK		1	/* write */
#define WDS_HCR			2	/* write */

/* WDS_STAT (read) defs */
#define WDSS_IRQ		0x80
#define WDSS_RDY		0x40
#define WDSS_REJ		0x20
#define WDSS_INIT		0x10

/* WDS_IRQSTAT (read) defs */
#define WDSI_MASK		0xc0
#define WDSI_ERR		0x00
#define WDSI_MFREE		0x80
#define WDSI_MSVC		0xc0

/* WDS_CMD (write) defs */
#define WDSC_NOOP		0x00
#define WDSC_INIT		0x01
#define WDSC_DISUNSOL		0x02
#define WDSC_ENAUNSOL		0x03
#define WDSC_IRQMFREE		0x04
#define WDSC_SCSIRESETSOFT	0x05
#define WDSC_SCSIRESETHARD	0x06
#define WDSC_MSTART(m)		(0x80 + (m))
#define WDSC_MMSTART(m)		(0xc0 + (m))

/* WDS_HCR (write) defs */
#define WDSH_IRQEN		0x08
#define WDSH_DRQEN		0x04
#define WDSH_SCSIRESET		0x02
#define WDSH_ASCRESET		0x01

#define WDS_NSEG	17

struct wds_scat_gath {
	physlen seg_len;
	physaddr seg_addr;
};

struct wds_cmd {
	u_char opcode;
	u_char targ;
	u_char scb[12];
	u_char stat;
	u_char venderr;
	physlen len;
	physaddr data;
	physaddr link;
	u_char write;
	u_char xx[6];
};

struct wds_scb {
	struct wds_cmd cmd;
	struct wds_cmd sense;

	struct wds_scat_gath scat_gath[WDS_NSEG];
	struct scsi_sense_data sense_data;

	TAILQ_ENTRY(wds_scb) chain;
	struct wds_scb *nexthash;
	u_long hashkey;
	struct scsipi_xfer *xs;
	int flags;
#define	SCB_ALLOC	0x01
#define	SCB_ABORT	0x02
#ifdef WDSDIAG
#define	SCB_SENDING	0x04
#endif
#define	SCB_POLLED	0x08
#define	SCB_SENSE	0x10
#define	SCB_DONE	0x20	/* for internal commands only */
#define	SCB_BUFFER	0x40
	int timeout;

	/*
	 * DMA maps used by the SCB.  These maps are created in
	 * wds_init_scb().
	 */

	/*
	 * The DMA map maps an individual SCB.  This map is permanently
	 * loaded in wds_init_scb().
	 */
	bus_dmamap_t	dmamap_self;

	/*
	 * This map maps the buffer involved in the transfer.
	 * Its contents are loaded into "scat_gath" above.
	 */
	bus_dmamap_t	dmamap_xfer;
};

#define WDSX_SCSICMD		0x00
#define WDSX_SCSISG		0x01
#define WDSX_OPEN_RCVBUF	0x80
#define WDSX_RCV_CMD		0x81
#define WDSX_RCV_DATA		0x82
#define WDSX_RCV_DATASTAT	0x83
#define WDSX_SND_DATA		0x84
#define WDSX_SND_DATASTAT	0x85
#define WDSX_SND_CMDSTAT	0x86
#define WDSX_READINIT		0x88
#define WDSX_READSCSIID		0x89
#define WDSX_SETUNSOLIRQMASK	0x8a
#define WDSX_GETUNSOLIRQMASK	0x8b
#define WDSX_GETFIRMREV		0x8c
#define WDSX_EXECDIAG		0x8d
#define WDSX_SETEXECPARM	0x8e
#define WDSX_GETEXECPARM	0x8f

struct wds_mbx_out {
	u_char cmd;
	physaddr scb_addr;
};

struct wds_mbx_in {
	u_char stat;
	physaddr scb_addr;
};

/*
 * mbo.cmd values
 */
#define	WDS_MBO_FREE		0x0	/* MBO entry is free */
#define	WDS_MBO_START		0x1	/* MBO activate entry */

/*
 * mbi.stat values
 */
#define	WDS_MBI_FREE		0x00	/* MBI entry is free */
#define WDS_MBI_OK		0x01	/* completed without error */
#define WDS_MBI_OKERR		0x02	/* completed with error */
#define WDS_MBI_ETIME		0x04
#define WDS_MBI_ERESET		0x05
#define WDS_MBI_ETARCMD		0x06
#define WDS_MBI_ERESEL		0x80
#define WDS_MBI_ESEL		0x81
#define WDS_MBI_EABORT		0x82
#define WDS_MBI_ESRESET		0x83
#define WDS_MBI_EHRESET		0x84

struct wds_setup {
	u_char opcode;
	u_char scsi_id;
	u_char buson_t;
	u_char busoff_t;
	u_char xx;
	physaddr mbaddr;
	u_char nomb;
	u_char nimb;
};

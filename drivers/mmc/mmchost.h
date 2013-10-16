
#define SUBPARTITION_PER_PARTITION 4	/* 4 sub partitions per partition */
#define PARTITONS_PER_DISK 4	/* 4 partitions per disk */
#define MINOR_PER_DISK  1	/* one additional minor to point to */

/**
 * We can have multiple MMC host controller present on the hardware. The MINIX
 * approach to handle this is to run a driver for each instance. Every driver
 * will therefore be stated with an "instance" id and the rest of the code here
 * will assume a single host controller to be present.
 *
 * The SD specification allows multiple cards to be attached to a single host
 * controller using the same lines. I recommend reading SD Specifications Part 1
 * Physical layer Simplified Specification chapter 3 about SD Memory Card system
 * concepts if you want to get a better understanding of this.
 *
 * In practice an MMC host will usually have a single slot attached to it and that
 * Slot may or may not contain a card. On sudden card removal we might want to
 * keep track of the last inserted card and we might therefore at later stage
 * add an additional "last_card" attribute to the card.
 *
 * The following diagram shows the structure that will be used to modulate the
 * hardware written in umlwiki  syntax.
 *
 * [/host/
 *   +instance:int] 1 --- 0..4 [ /slot/
 *                                +card_detect:func ] 1 --- 0..1 [ /card/ ]
 *                                 `
 */

#define MAX_SD_SLOTS 4

struct mmc_host;

//TODO Add more modes like INACTIVE STATE and such
#define SD_MODE_UNINITIALIZED 0
#define SD_MODE_CARD_IDENTIFICATION 1
#define SD_MODE_DATA_TRANSFER_MODE 2

struct sd_card_regs
{
	uint32_t cid[4];	/* Card Identification */
	uint32_t rca;		/* Relative card address */
	uint32_t dsr;		/* Driver stage register */
	uint32_t csd[4];	/* Card specific data */
	uint32_t scr[2];	/* SD configuration */
	uint32_t ocr;		/* Operation conditions */
	uint32_t ssr[5];	/* SD Status */
	uint32_t csr;		/* Card status */
};

#define  RESP_LEN_48_CHK_BUSY (3<<0)
#define  RESP_LEN_48		  (2<<0)
#define  RESP_LEN_136		  (1<<0)
#define  RESP_NO_RESPONSE	  (0<<0)

#define  DATA_NONE	  (0)
#define  DATA_READ	  (1)
#define  DATA_WRITE	  (2)

/* struct representing an mmc command */
struct mmc_command
{
	uint32_t cmd;
	uint32_t args;
	uint32_t resp_type;
	uint32_t data_type;
	uint32_t resp[4];
	unsigned char *data;
	uint32_t data_len;
};

/* structure representing an SD card */
struct sd_card
{
	/* pointer back to the SD slot for convenience */
	struct sd_slot *slot;

	struct sd_card_regs regs;

	/* some helpers (data comming from the csd) */
	uint32_t blk_size;
	uint32_t blk_count;

	/* drive state: deaf, initialized, dead */
	unsigned state;

	/* MINIX/block driver related things */
	int open_ct;		/* in-use count */

	/* 1 disks + 4 partitions and 16 possible sub partitions */
	struct device part[MINOR_PER_DISK + PARTITONS_PER_DISK];
	struct device subpart[PARTITONS_PER_DISK * SUBPARTITION_PER_PARTITION];
};

/* structure representing an SD slot */
struct sd_slot
{
	/* pointer back to the host for convenience */
	struct mmc_host *host;

	unsigned state;
	struct sd_card card;
};

/* structure for the host controller */
struct mmc_host
{
	/* MMC host configuration */
	int (*host_set_instance) (struct mmc_host * host, int instance);
	/* MMC host configuration */
	int (*host_init) (struct mmc_host * host);
	/* Set log level */
	void (*set_log_level) (int level);
	/* Host controller reset */
	int (*host_reset) (struct mmc_host * host);
	/* Card detection (binary yes/no) */
	int (*card_detect) (struct sd_slot * slot);
	/* Perform card detection e.g. card type */
	struct sd_card *(*card_initialize) (struct sd_slot * slot);
	/* Release the card */
	int (*card_release) (struct sd_card * card);

	/* Additional hardware interrupts */
	void (*hw_intr) (unsigned int irqs);

	/* read count blocks into existing buf */
	int (*read) (struct sd_card * card,
	    uint32_t blknr, uint32_t count, unsigned char *buf);

	/* write count blocks */
	int (*write) (struct sd_card * card,
	    uint32_t blknr, uint32_t count, unsigned char *buf);

	/* up to 4 slots with 4 SD cards */
	struct sd_slot slot[MAX_SD_SLOTS];
};

#if 0
/* Command execution */
int (*send_cmd) (struct sd_card * card, struct mmc_command *);

/* struct representing an mmc command */
struct mmc_command
{
	uint32_t cmd;
	uint32_t args;
	uint32_t resp[4];
	unsigned char *data;
	uint32_t data_len;
};
#endif

/* Hack done for driver registration */
void host_initialize_host_structure_mmchs(struct mmc_host *host);
void host_initialize_host_structure_dummy(struct mmc_host *host);

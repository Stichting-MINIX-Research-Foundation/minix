/* kernel headers */
#include <minix/blockdriver.h>
#include <minix/minlib.h>
#include <minix/log.h>

/* usr headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>

/* local headers */
#include "mmchost.h"
#include "sdmmcreg.h"

/*
 * Define a structure to be used for logging
 */
static struct log log = {
	.name = "mmc_host_memory",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* This is currently a dummy driver using an in-memory structure */
#define DUMMY_SIZE_IN_BLOCKS 0xFFFFFu
#define DUMMY_BLOCK_SIZE 512
static char *dummy_data = NULL;

static struct sd_card *
init_dummy_sdcard(struct sd_slot *slot)
{
	int i;
	struct sd_card *card;

	assert(slot != NULL);

	log_info(&log, "Using a dummy card \n");
	if (dummy_data == NULL) {
		dummy_data = malloc(DUMMY_BLOCK_SIZE * DUMMY_SIZE_IN_BLOCKS);
		if (dummy_data == NULL) {
			panic
			    ("Failed to allocate data for dummy mmc driver\n");
		}
	}

	card = &slot->card;
	memset(card, 0, sizeof(struct sd_card));
	card->slot = slot;

	for (i = 0; i < MINOR_PER_DISK + PARTITONS_PER_DISK; i++) {
		card->part[i].dv_base = 0;
		card->part[i].dv_size = 0;
	}

	for (i = 0; i < PARTITONS_PER_DISK * SUBPARTITION_PER_PARTITION; i++) {
		card->subpart[i].dv_base = 0;
		card->subpart[i].dv_size = 0;
	}

	card->part[0].dv_base = 0;
	card->part[0].dv_size = DUMMY_BLOCK_SIZE * DUMMY_SIZE_IN_BLOCKS;
	return card;
}

int
dummy_host_init(struct mmc_host *host)
{
	return 0;
}

void
dummy_set_log_level(int level)
{
	if (level >= 0 && level <= 4) {
		log.log_level = level;
	}
}

int
dummy_host_set_instance(struct mmc_host *host, int instance)
{
	log_info(&log, "Using instance number %d\n", instance);
	if (instance != 0) {
		return EIO;
	}
	return OK;
}

int
dummy_host_reset(struct mmc_host *host)
{
	return 0;
}

int
dummy_card_detect(struct sd_slot *slot)
{
	return 1;
}

struct sd_card *
dummy_card_initialize(struct sd_slot *slot)
{
	slot->card.blk_size = DUMMY_BLOCK_SIZE;
	slot->card.blk_count = DUMMY_SIZE_IN_BLOCKS;
	slot->card.state = SD_MODE_DATA_TRANSFER_MODE;

	memset(slot->card.part, 0, sizeof(slot->card.part));
	memset(slot->card.subpart, 0, sizeof(slot->card.subpart));
	slot->card.part[0].dv_base = 0;
	slot->card.part[0].dv_size = DUMMY_BLOCK_SIZE * DUMMY_SIZE_IN_BLOCKS;
	return &slot->card;
}

int
dummy_card_release(struct sd_card *card)
{
	assert(card->open_ct == 1);
	card->open_ct--;
	card->state = SD_MODE_UNINITIALIZED;
	/* TODO:Set card state */
	return OK;
}

/* read count blocks into existing buf */
int
dummy_host_read(struct sd_card *card,
    uint32_t blknr, uint32_t count, unsigned char *buf)
{
	memcpy(buf, &dummy_data[blknr * DUMMY_BLOCK_SIZE],
	    count * DUMMY_BLOCK_SIZE);
	return OK;
}

/* write count blocks */
int
dummy_host_write(struct sd_card *card,
    uint32_t blknr, uint32_t count, unsigned char *buf)
{
	memcpy(&dummy_data[blknr * DUMMY_BLOCK_SIZE], buf,
	    count * DUMMY_BLOCK_SIZE);
	return OK;
}

void
host_initialize_host_structure_dummy(struct mmc_host *host)
{
	/* Initialize the basic data structures host slots and cards */
	int i;

	host->host_set_instance = dummy_host_set_instance;
	host->host_init = dummy_host_init;
	host->set_log_level = dummy_set_log_level;
	host->host_reset = dummy_host_reset;
	host->card_detect = dummy_card_detect;
	host->card_initialize = dummy_card_initialize;
	host->card_release = dummy_card_release;
	host->read = dummy_host_read;
	host->write = dummy_host_write;

	/* initialize data structures */
	for (i = 0; i < sizeof(host->slot) / sizeof(host->slot[0]); i++) {
		// @TODO set initial card and slot state
		host->slot[i].host = host;
		host->slot[i].card.slot = &host->slot[i];
	}
	init_dummy_sdcard(&host->slot[0]);
}

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <minix/blockdriver.h>
#include "mmclog.h"
#include "mmchost.h"
/*
 * Define a structure to be used for logging
 */
static struct mmclog log= {
		.name = "mmc_host",
		.log_level = LEVEL_TRACE,
		.log_func = default_log };

#define DUMMY_SIZE_IN_BLOCKS 0xFFFu
/* @TODO rename sector into BLOCK_SIZE */
#define DUMMY_BLOCK_SIZE 512

static char dummy_data[DUMMY_BLOCK_SIZE * DUMMY_SIZE_IN_BLOCKS];

static struct sd_card *init_dummy_sdcard(struct sd_slot *slot)
{
	int i;
	struct sd_card *card;

	assert(slot != NULL);

	card= &slot->card;
	card->slot= slot;

	memset(card, 0, sizeof(struct sd_card));
	for (i= 0; i < MINOR_PER_DISK + PARTITONS_PER_DISK; i++) {
		card->part[i].dv_base= 0;
		card->part[i].dv_size= 0;
	}

	for (i= 0; i < PARTITONS_PER_DISK * SUBPARTITION_PER_PARTITION; i++) {
		card->subpart[i].dv_base= 0;
		card->subpart[i].dv_size= 0;
	}
	card->part[0].dv_base= 0;
	card->part[0].dv_size= DUMMY_BLOCK_SIZE * DUMMY_SIZE_IN_BLOCKS ;
	return card;
}

int mmchs_host_init(struct mmc_host *host)
{
	return 0;
}

int mmchs_host_set_instance(struct mmc_host *host, int instance)
{
	mmc_log_info(&log, "Using instance number %d\n", instance);
	if (instance != 0) {
		return EIO;
	}
	return OK;
}

int mmchs_host_reset(struct mmc_host *host)
{
	return 0;
}

int mmchs_card_detect(struct sd_slot* slot)
{
	/*TODO:Set card state  */
	return 1;
}

struct sd_card * mmchs_card_initialize(struct sd_slot* slot)
{
	init_dummy_sdcard(slot);
	/* TODO: Add dummy data */
	slot->card.blk_size= DUMMY_BLOCK_SIZE;
	slot->card.blk_count= DUMMY_SIZE_IN_BLOCKS;
	return &slot->card;
}

int mmchs_card_release(struct sd_card* card)
{
	assert(card->open_ct == 1);
	card->open_ct--;
	/*TODO:Set card state  */
	return OK;
}

/* read count blocks into existing buf */
int mmchs_host_read(struct sd_card *card,
		uint32_t blknr,
		uint32_t count,
		unsigned char * buf)
{
	memcpy(buf, &dummy_data[blknr * DUMMY_BLOCK_SIZE], count * DUMMY_BLOCK_SIZE);
	return OK;
}

/* write count blocks */
int mmchs_host_write(struct sd_card *card,
		uint32_t blknr,
		uint32_t count,
		unsigned char * buf)
{
	memcpy(&dummy_data[blknr * DUMMY_BLOCK_SIZE], buf, count * DUMMY_BLOCK_SIZE);
	return OK;
}

void mmchs_host_initialize_host_structure(struct mmc_host * host)
{
	/* Initialize the basic data structures host slots and cards */
	int i;

	host->host_set_instance= mmchs_host_set_instance;
	host->host_init= mmchs_host_init;
	host->host_reset= mmchs_host_reset;
	host->card_detect= mmchs_card_detect;
	host->card_initialize= mmchs_card_initialize;
	host->card_release= mmchs_card_release;
	host->read= mmchs_host_read;
	host->write= mmchs_host_write;

	/* initialize data structures */
	for (i= 0; i < sizeof(host->slot) /sizeof(host->slot[0]); i++) {
		//@TODO set initial card and slot state
		host->slot[i].host= host;
		host->slot[i].card.slot= &host->slot[i];
	}
}

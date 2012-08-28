#include "mmclog.h"
/*
 * Define a structure to be used for logging
 */
static struct mmclog log= {
		.name = "mmc_host",
		.log_level = LEVEL_TRACE,
		.log_func = default_log };



void mmchs_host_initialize_host_structure(struct mmc_host * host)
{
	/* Initialize the basic data structures host slots and cards */
	int i;

	host->host_set_instance =mmchs_host_set_instance;
	host->host_init =mmchs_host_init;
	host->host_reset = mmchs_host_reset;
	host->card_detect = mmchs_card_detect;
	host->card_initialize = mmchs_card_initialize;
	host->card_release =mmchs_card_release;

	/* initialize data structures */
	for (i =0; i < MAX_SDLOTS ; i++){
		//@TODO set initial card and slot state
		host->slot[i].host = host;
		host->slot[i].card.slot = & host->slot[i];
	}
}

#define DUMMY_SIZE_IN_BLOCKS 0xFFFu
#define SECTOR_SIZE 512
static char dummy_data[SECTOR_SIZE * DUMMY_SIZE_IN_BLOCKS];

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
	card->part[0].dv_size= SECTOR_SIZE * DUMMY_SIZE_IN_BLOCKS;
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
	return &slot->card;
}


int mmchs_card_release(struct sd_card* card)
{
	assert(card->open_ct == 1);
	card->open_ct--;
	/*TODO:Set card state  */
	return OK;
}

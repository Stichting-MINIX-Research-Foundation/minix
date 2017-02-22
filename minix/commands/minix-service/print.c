
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <lib.h>
#include <minix/config.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/rs.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>
#include <paths.h>
#include <minix/sef.h>
#include <minix/dmap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <configfile.h>

#include <machine/archtypes.h>
#include <minix/timers.h>
#include <err.h>

#include "config.h"
#include "proto.h"
#include "parse.h"

#define MAXDEPTH 10

static struct {
	const char *field, *str;
} configstack[MAXDEPTH];

int depth = 0;

void printstack(void)
{
	int i;
	for(i = 0; i < depth; i++)
		printf("%s %s,", configstack[i].field,
			configstack[i].str);
}

void print(const char *field, const char *str)
{
	printstack();
	printf("%s %s\n", field, str);
}

void push(const char *field, const char *str)
{
	assert(depth < MAXDEPTH);
	configstack[depth].field = field;
	configstack[depth].str = str;
	depth++;
	printstack();
	printf("\n");
}

int main(int argc, char **argv)
{
	struct rs_config config;
	const char *label;
	uint16_t sub_vid, sub_did;
        int id;

	if(argc != 2) {
		fprintf(stderr, "usage: %s <config>\n", argv[0]);
		return 1;
	}

	memset(&config, 0, sizeof(config));
	if(!(label = parse_config(NULL, 1, argv[1], &config)))
		errx(1, "parse_config failed");

	push(KW_SERVICE, label);
	if(config.type) push(KW_TYPE, config.type);

	if(config.descr) push(KW_DESCR, config.descr);
	if(config.rs_start.rss_nr_pci_id > 0) {
		printstack();
		printf("%s %s ", KW_PCI, KW_DEVICE);
		for(id = 0; id < config.rs_start.rss_nr_pci_id; id++) {
			sub_vid = config.rs_start.rss_pci_id[id].sub_vid;
			sub_did = config.rs_start.rss_pci_id[id].sub_did;
			/*
			 * The PCI driver interprets each of these two fields
			 * individually, so we must print them even if just one
			 * of them is set.  Correct matching of just one of
			 * the fields may be hard to do from a script though,
			 * so driver writers are advised to specify either both
			 * or neither of these two fields.
			 */
			if (sub_vid != NO_SUB_VID || sub_did != NO_SUB_DID)
				printf("%04X:%04X/%04X:%04X ",
					config.rs_start.rss_pci_id[id].vid,
					config.rs_start.rss_pci_id[id].did,
					sub_vid, sub_did);
			else
				printf("%04X:%04X ",
					config.rs_start.rss_pci_id[id].vid,
					config.rs_start.rss_pci_id[id].did);
		}
		printf("\n");
	}
}


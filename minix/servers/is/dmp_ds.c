#include "inc.h"
#include "../ds/store.h"

#define LINES 22

static struct data_store noxfer_ds_store[NR_DS_KEYS];

void
data_store_dmp(void)
{
  struct data_store *p;
  static int prev_i = 0;
  int i, n = 0;

  if (getsysinfo(DS_PROC_NR, SI_DATA_STORE, noxfer_ds_store, sizeof(noxfer_ds_store)) != OK) {
	printf("Error obtaining table from DS. Perhaps recompile IS?\n");
	return;
  }

  printf("Data store contents:\n");
  printf("-slot- -----------key----------- -----owner----- ---type--- ----value---\n");
  for(i = prev_i; i < NR_DS_KEYS && n < LINES; i++) {
	p = &noxfer_ds_store[i];
	if(!(p->flags & DSF_IN_USE))
		continue;

	printf("%6d %-25s %-15s ", i, p->key, p->owner);
	switch(p->flags & DSF_MASK_TYPE) {
	case DSF_TYPE_U32:
		printf("%-10s %12u\n", "U32", p->u.u32);
		break;
	case DSF_TYPE_STR:
		printf("%-10s %12s\n", "STR", (char*) p->u.mem.data);
		break;
	case DSF_TYPE_MEM:
		printf("%-10s %12zu\n", "MEM", p->u.mem.length);
		break;
	case DSF_TYPE_LABEL:
		printf("%-10s %12u\n", "LABEL", p->u.u32);
		break;
	default:
		return;
	}

	n++;
  }

  if (i >= NR_DS_KEYS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}


#include "inc.h"
#include "../ds/store.h"

#define LINES 22

PRIVATE struct data_store ds_store[NR_DS_KEYS];

PUBLIC void data_store_dmp()
{
  struct data_store *p;
  static int prev_i = 0;
  int r, i, n = 0;

  if((r=getsysinfo(DS_PROC_NR, SI_DATA_STORE, ds_store)) != OK) {
	printf("Couldn't talk to DS: %d.\n", r);
	return;
  }

  printf("Data store contents:\n");
  printf("-slot- ------key------ -----owner----- ---type--- ----value---\n");
  for(i = prev_i; i < NR_DS_KEYS && n < LINES; i++) {
	p = &ds_store[i];
	if(!(p->flags & DSF_IN_USE))
		continue;

	printf("%6d %-15s %-15s ", i, p->key, p->owner);
	switch(p->flags & DSF_MASK_TYPE) {
	case DSF_TYPE_U32:
		printf("%-10s %12u\n", "U32", p->u.u32);
		break;
	case DSF_TYPE_STR:
		printf("%-10s %12s\n", "STR", p->u.string);
		break;
	case DSF_TYPE_MEM:
		printf("%-10s %12u\n", "MEM", p->u.mem.length);
		break;
	case DSF_TYPE_MAP:
		printf("%-10s %9u/%3u\n", "MAP", p->u.map.length,
			p->u.map.sindex);
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


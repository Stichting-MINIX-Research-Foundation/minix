
#include "pm.h"

#define ASYN_NR	100
PRIVATE asynmsg_t msgtable[ASYN_NR];
PRIVATE int first_slot= 0, next_slot= 0;

PUBLIC int asynsend(dst, mp)
endpoint_t dst;
message *mp;
{
	int r, src_ind, dst_ind;
	unsigned flags;

	/* Update first_slot */
	for (; first_slot < next_slot; first_slot++)
	{
		flags= msgtable[first_slot].flags;
		if ((flags & (AMF_VALID|AMF_DONE)) == (AMF_VALID|AMF_DONE))
		{
			if (msgtable[first_slot].result != OK)
			{
				printf(
			"asynsend: found completed entry %d with error %d\n",
					first_slot,
					msgtable[first_slot].result);
			}
			continue;
		}
		if (flags != AMF_EMPTY)
			break;
	}

	if (first_slot >= next_slot)
	{
		/* Reset first_slot and next_slot */
		next_slot= first_slot= 0;
	}

	if (next_slot >= ASYN_NR)
	{
		/* Tell the kernel to stop processing */
		r= senda(NULL, 0);
		if (r != OK)
			panic(__FILE__, "asynsend: senda failed", r);

		dst_ind= 0;
		for (src_ind= first_slot; src_ind<next_slot; src_ind++)
		{
			flags= msgtable[src_ind].flags;
			if ((flags & (AMF_VALID|AMF_DONE)) ==
				(AMF_VALID|AMF_DONE))
			{
				if (msgtable[src_ind].result != OK)
				{
					printf(
			"asynsend: found completed entry %d with error %d\n",
						src_ind,
						msgtable[src_ind].result);
				}
				continue;
			}
			if (flags == AMF_EMPTY)
				continue;
#if 0
			printf("asynsend: copying entry %d to %d\n",
				src_ind, dst_ind);
#endif
			if (src_ind != dst_ind)
				msgtable[dst_ind]= msgtable[src_ind];
			dst_ind++;
		}
		first_slot= 0;
		next_slot= dst_ind;
		if (next_slot >= ASYN_NR)
			panic(__FILE__, "asynsend: msgtable full", NO_NUM);
	}

	msgtable[next_slot].dst= dst;
	msgtable[next_slot].msg= *mp;
	msgtable[next_slot].flags= AMF_VALID;	/* Has to be last. The kernel 
					 	 * scans this table while we
						 * are sleeping.
					 	 */
	next_slot++;

	/* Tell the kernel to rescan the table */
	return senda(msgtable+first_slot, next_slot-first_slot);
}


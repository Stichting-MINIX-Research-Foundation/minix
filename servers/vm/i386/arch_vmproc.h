
#include <archtypes.h>

struct vm_arch {
	struct mem_map	vm_seg[NR_LOCAL_SEGS];	/* text, data, stack */

	/* vm_data_top points to top of data address space, as visible
	 * from user-space, in bytes.
	 * for segments processes this is the same 
	 * as the top of vm_seg[S] segment. for paged processes this
	 * can be much higher (so more memory is available above the
	 * stack).
	 */
	u32_t		vm_data_top;	/* virtual process space in bytes */
};

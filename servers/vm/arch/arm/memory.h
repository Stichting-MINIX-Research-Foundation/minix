#include <machine/vm.h>

/* And what is the highest addressable piece of memory, when in paged
 * mode?
 */
#define VM_DATATOP	kernel_boot_info.user_end
#define VM_STACKTOP	kernel_boot_info.user_sp

#define SLAB_PAGESIZE	ARM_PAGE_SIZE
#define VM_PAGE_SIZE	ARM_PAGE_SIZE

#define CLICKSPERPAGE (ARM_PAGE_SIZE/CLICK_SIZE)

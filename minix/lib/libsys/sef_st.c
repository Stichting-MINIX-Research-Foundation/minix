#include "syslib.h"
#include <assert.h>
#include <string.h>
#include <machine/archtypes.h>
#include <minix/timers.h>
#include <minix/sysutil.h>
#include <minix/vm.h>

#include "kernel/config.h"
#include "kernel/const.h"
#include "kernel/type.h"
#include "kernel/proc.h"

EXTERN endpoint_t sef_self_endpoint;

/* SEF Live update prototypes for sef_receive(). */
void do_sef_st_before_receive(void);

/*===========================================================================*
 *      	            do_sef_st_before_receive                         *
 *===========================================================================*/
void do_sef_st_before_receive(void)
{
}

/*===========================================================================*
 *      	            sef_copy_state_region_ctl                        *
 *===========================================================================*/
int sef_copy_state_region_ctl(sef_init_info_t *info, vir_bytes *src_address, vir_bytes *dst_address) {
    if(info->copy_flags & SEF_COPY_DEST_OFFSET) {
        *dst_address += sef_llvm_get_ltckpt_offset();
    }
    if(info->copy_flags & SEF_COPY_SRC_OFFSET) {
        *src_address += sef_llvm_get_ltckpt_offset();
    }
#if STATE_TRANS_DEBUG
    printf("sef_copy_state_region_ctl. copy_flags:\nSEF_COPY_DEST_OFFSET\t%d\nSEF_COPY_SRC_OFFSET\t%d\nSEF_COPY_NEW_TO_NEW\t%d\nSEF_COPY_OLD_TO_NEW\t%d\n", info->copy_flags & SEF_COPY_DEST_OFFSET ? 1 : 0, 
            info->copy_flags & SEF_COPY_SRC_OFFSET ? 1 : 0, info->copy_flags & SEF_COPY_NEW_TO_NEW ? 1 : 0, info->copy_flags & SEF_COPY_OLD_TO_NEW ? 1 : 0);
#endif
    if(info->copy_flags & SEF_COPY_NEW_TO_NEW)
        return 1;
    return 0;
}

/*===========================================================================*
 *      	             sef_copy_state_region                           *
 *===========================================================================*/
int sef_copy_state_region(sef_init_info_t *info,
    vir_bytes address, size_t size, vir_bytes dst_address, int may_have_holes)
{
  vir_bytes base, top, target;
  struct vm_region_info vri;
  int r;

  base = address;

  if(sef_copy_state_region_ctl(info, &address, &dst_address)) {
#if STATE_TRANS_DEBUG
      printf("sef_copy_state_region: memcpy %d bytes, addr = 0x%08x -> 0x%08x...\n",
              size, address, dst_address);
#endif
      /* memcpy region from current state */
      memcpy((void*) dst_address, (void *)address, size);
  } else if (may_have_holes && sef_self_endpoint != VM_PROC_NR &&
    vm_info_region(info->old_endpoint, &vri, 1, &base) == 1) {
      /* Perform a safe copy of a region of the old state.  The section may
       * contain holes, so ask VM for the actual regions within the data
       * section and transfer each one separately.  The alternative, just
       * copying until a page fault happens, is not possible in the multi-
       * component-with-VM live update case, where VM may not receive page
       * faults during the live update window.  For now, we use the region
       * iteration approach for the data section only; other cases have not
       * been tested, but may work as well.
       */
#if STATE_TRANS_DEBUG
      printf("sef_copy_state_region: copying %d bytes, addr = 0x%08x -> "
        "0x%08x, gid = %d, source = %d, with holes...\n", size, address,
        dst_address, SEF_STATE_TRANSFER_GID, info->old_endpoint);
#endif

      /* The following is somewhat of a hack: the start of the data section
       * may in fact not be page-aligned and may be part of the last page of
       * of the preceding (text) section.  Therefore, if the first region we
       * find starts above the known base address, blindly copy the area in
       * between.
       */
      if (vri.vri_addr > address) {
          if ((r = sys_safecopyfrom(info->old_endpoint, SEF_STATE_TRANSFER_GID,
            address, dst_address, vri.vri_addr - address)) != OK) {
#if STATE_TRANS_DEBUG
              printf("sef_copy_state_region: sys_safecopyfrom failed\n");
#endif
              return r;
          }
      }

      top = address + size;
      do {
          assert(vri.vri_addr >= address);
          if (vri.vri_addr >= top)
              break;
          if (vri.vri_length > top - vri.vri_addr)
              vri.vri_length = top - vri.vri_addr;
          target = dst_address + (vri.vri_addr - address);
          if ((r = sys_safecopyfrom(info->old_endpoint,
            SEF_STATE_TRANSFER_GID, vri.vri_addr, target,
            vri.vri_length)) != OK) {
#if STATE_TRANS_DEBUG
              printf("sef_copy_state_region: sys_safecopyfrom failed\n");
#endif
              return r;
          }
          /* Save on a VM call if the next address is already too high. */
          if (base >= top)
              break;
      } while (vm_info_region(info->old_endpoint, &vri, 1, &base) == 1);
  } else {
      /* Perform a safe copy of a region of the old state, without taking into
       * account any holes.  This is the default for anything but the data
       * section, with a few additioanl exceptions:  VM can't query VM, so
       * simply assume there are no holes;  also, if we fail to get one region
       * for the old process (and this is presumably possible if its heap is
       * so small it fits in the last text page, see above), we also just
       * blindly copy over the entire data section.
       */
#if STATE_TRANS_DEBUG
      printf("sef_copy_state_region: copying %d bytes, addr = 0x%08x -> "
        "0x%08x, gid = %d, source = %d, without holes...\n", size, address,
        dst_address, SEF_STATE_TRANSFER_GID, info->old_endpoint);
#endif
      if ((r = sys_safecopyfrom(info->old_endpoint, SEF_STATE_TRANSFER_GID,
        address, dst_address, size)) != OK) {
#if STATE_TRANS_DEBUG
          printf("sef_copy_state_region: sys_safecopyfrom failed\n");
#endif
          return r;
      }
  }

  return OK;
}

/*===========================================================================*
 *                          sef_old_state_table_lookup                       *
 *===========================================================================*/
 int sef_old_state_table_lookup(sef_init_info_t *info, void *addr)
{
  struct priv old_priv;
  int r;

  if ((r = sys_getpriv(&old_priv, info->old_endpoint)) != OK) {
      printf("ERROR. sys_getpriv() failed.\n");
      return r;
  }

  if (sef_copy_state_region(info, old_priv.s_state_table
    , sef_llvm_state_table_size(), (vir_bytes) addr, FALSE /*may_have_holes*/))
  {
      printf("ERROR. state table transfer failed\n");
      return EGENERIC;
  }

  return OK;
}

/*===========================================================================*
 *                       sef_old_state_table_lookup_opaque                   *
 *===========================================================================*/
int sef_old_state_table_lookup_opaque(void *info_opaque, void *addr)
{
  assert(info_opaque != NULL && "Invalid info_opaque pointer.");
  return sef_old_state_table_lookup((sef_init_info_t *)(info_opaque), addr);
}

/*===========================================================================*
 *                         sef_copy_state_region_opaque                      *
 *===========================================================================*/
int sef_copy_state_region_opaque(void *info_opaque, uint32_t address,
	size_t size, uint32_t dst_address)
{
  assert(info_opaque != NULL && "Invalid info_opaque pointer.");
  return sef_copy_state_region((sef_init_info_t *)(info_opaque),
      (vir_bytes) address, size, (vir_bytes) dst_address,
      FALSE /*may_have_holes*/);
}

/*===========================================================================*
 *                            sef_st_state_transfer                          *
 *===========================================================================*/
int sef_st_state_transfer(sef_init_info_t *info)
{
    return sef_llvm_state_transfer(info);
}


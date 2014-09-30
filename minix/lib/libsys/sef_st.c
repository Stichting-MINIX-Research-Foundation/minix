#include "syslib.h"
#include <assert.h>
#include <string.h>
#include <machine/archtypes.h>
#include <minix/timers.h>
#include <minix/sysutil.h>

#include "kernel/config.h"
#include "kernel/const.h"
#include "kernel/type.h"
#include "kernel/proc.h"

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
    vir_bytes address, size_t size, vir_bytes dst_address)
{
  int r;
  if(sef_copy_state_region_ctl(info, &address, &dst_address)) {
#if STATE_TRANS_DEBUG
      printf("sef_copy_state_region: memcpy %d bytes, addr = 0x%08x -> 0x%08x...\n",
              size, address, dst_address);
#endif
      /* memcpy region from current state */
      memcpy((void*) dst_address, (void *)address, size);
  } else {
#if STATE_TRANS_DEBUG
      printf("sef_copy_state_region: copying %d bytes, addr = 0x%08x -> 0x%08x, gid = %d, source = %d...\n",
              size, address, dst_address, SEF_STATE_TRANSFER_GID, info->old_endpoint);
#endif
      /* Perform a safe copy of a region of the old state. */
      if((r = sys_safecopyfrom(info->old_endpoint, SEF_STATE_TRANSFER_GID, address,
        dst_address, size)) != OK) {
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
    , sef_llvm_state_table_size(), (vir_bytes) addr))
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
      (vir_bytes) address, size, (vir_bytes) dst_address);
}

/*===========================================================================*
 *                            sef_st_state_transfer                          *
 *===========================================================================*/
int sef_st_state_transfer(sef_init_info_t *info)
{
    return sef_llvm_state_transfer(info);
}


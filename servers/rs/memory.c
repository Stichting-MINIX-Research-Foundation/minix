/* This file contains memory management routines for RS.
 *
 * Changes:
 *   Nov 22, 2009: Created    (Cristiano Giuffrida)
 */

#include "inc.h"
#include "../../kernel/const.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"

EXTERN char *_brksize;

PRIVATE char * _rs_startbrksize = NULL;
PRIVATE char * _rs_endbrksize = NULL;

#define munmap _munmap
#define munmap_text _munmap_text
#include <sys/mman.h>
#undef munmap
#undef munmap_text

PUBLIC int unmap_ok = 0;

/*===========================================================================*
 *			      check_mem_available			     *
 *===========================================================================*/
PRIVATE int check_mem_available(char *new_brksize)
{
/* Check if enough memory is available to grow break size. */
  register struct mem_map *mem_sp, *mem_dp;
  vir_clicks sp_click, gap_base, sp_lower;
  int s;
  long base_of_stack, sp_delta;	/* longs avoid certain problems */
  vir_bytes sp;
  struct proc proc;
  vir_clicks data_clicks;

  /* Get stack pointer and pointers to data/stack segment maps. */
  if ((s=sys_getproc(&proc, SELF)) != OK) {
      return(s);
  }
  sp = proc.p_reg.sp;                    /* stack pointer */
  mem_dp = &proc.p_memmap[D];            /* pointer to data segment map */
  mem_sp = &proc.p_memmap[S];            /* pointer to stack segment map */

  /* Compute how many clicks the data segment is to become. */
  data_clicks = (vir_clicks) (CLICK_CEIL(new_brksize) >> CLICK_SHIFT)
	- mem_dp->mem_vir;

  /* See if stack size has gone negative (i.e., sp too close to 0xFFFF...) */
  base_of_stack = (long) mem_sp->mem_vir + (long) mem_sp->mem_len;
  sp_click = sp >> CLICK_SHIFT;	/* click containing sp */
  if (sp_click >= base_of_stack)
  {
	return(ENOMEM);	/* sp too high */
  }

  /* Compute size of gap between stack and data segments. */
  sp_delta = (long) mem_sp->mem_vir - (long) sp_click;
  sp_lower = (sp_delta > 0 ? sp_click : mem_sp->mem_vir);

  /* Add a safety margin for future stack growth. Impossible to do right. */
#define SAFETY_BYTES  (384 * sizeof(char *))
#define SAFETY_CLICKS ((vir_clicks) (CLICK_CEIL(SAFETY_BYTES) >> CLICK_SHIFT))
  gap_base = mem_dp->mem_vir + data_clicks + SAFETY_CLICKS;
  if (sp_lower < gap_base)
  {
	return(ENOMEM);	/* data and stack collided */
  }

  return(OK);
}
  
/*===========================================================================*
 *				rs_startup_sbrk				     *
 *===========================================================================*/
PUBLIC void* rs_startup_sbrk(size)
size_t size;                    /* the size to grow */
{
/* RS's own sbrk() used at startup. */
  void* addr;
  char* new_brksize;

  /* Check input for non-positive size or size overflows. */
  new_brksize = _brksize + size;
  if (size <= 0 || new_brksize < _brksize) {
      return( (char *) -1);
  }

  /* Check if enough memory is available. */
  if(check_mem_available(new_brksize) != OK) {
      return( (char *) -1);
  }
  
  /* Save initial break size. */
  if(_rs_startbrksize == NULL) {
      _rs_startbrksize = _brksize;
  }
  
  /* Set address and adjust break size. */
  addr = _brksize;
  _brksize = new_brksize;
  _rs_endbrksize = _brksize;

  return addr;
}

/*===========================================================================*
 *				rs_startup_sbrk_synch			     *
 *===========================================================================*/
PUBLIC void* rs_startup_sbrk_synch(size)
size_t size;                    /* the size to grow */
{
/* Synchronize RS's own sbrk() with the rest of the system right after
 * startup. We use the original sbrk() here.
 */
  void* addr;

  /* Restore original break size. */
  _brksize = _rs_startbrksize;

  /* Call original sbrk() and see if we observe the same effect. */
  addr = (void*)sbrk(size);
  if(_rs_startbrksize != addr) {
      printf("Unable to synch rs_startup_sbrk() and sbrk(): addr 0x%x!=0x%x\n",
          (int) _rs_startbrksize, (int) addr);
      return( (char *) -1);
  }
  if(_rs_endbrksize != _brksize) {
      printf("Unable to synch rs_startup_sbrk() and sbrk(): size 0x%x!=0x%x\n",
         (int) _rs_endbrksize, (int) _brksize);
      return( (char *) -1);
  }

  return addr;
}

/*===========================================================================*
 *				rs_startup_segcopy			     *
 *===========================================================================*/
PUBLIC int rs_startup_segcopy(src_proc, src_seg, dst_seg, dst_vir, bytes)
endpoint_t src_proc;            /* source process */
int src_seg;                    /* source memory segment */
int dst_seg;                    /* destination memory segment */
vir_bytes dst_vir;              /* destination virtual address */
phys_bytes bytes;               /* how many bytes */
{
/* Copy a process's T, D, S segment to RS's address space. Used at startup. */
  struct proc src_p, dst_p;
  phys_bytes src_phys, dst_phys;
  int s;

  /* Check input. */
  if((src_seg != T && src_seg != D && src_seg != S) || bytes <= 0) {
      return EINVAL;
  }

  /* We don't override normal behavior when not copying to our data segment. */
  if(dst_seg != D) {
      s = sys_vircopy(src_proc, src_seg, 0, SELF, dst_seg, dst_vir, bytes);
      return(s);
  }

  /* Get kernel process slot for both source and destination. */
  if ((s=sys_getproc(&src_p, src_proc)) != OK) {
      return(s);
  }
  if ((s=sys_getproc(&dst_p, SELF)) != OK) {
      return(s);
  }

  /* Map source address to physical address. */
  src_phys = (phys_bytes) src_p.p_memmap[src_seg].mem_phys << CLICK_SHIFT;

  /* Check if destination address is out of bounds or overflows. */
  if(dst_vir+bytes > (vir_bytes)_rs_endbrksize
      || dst_vir < (vir_bytes)_rs_startbrksize || dst_vir+bytes < dst_vir) {
      return EFAULT;
  }

  /* Map destination address to physical address. */
  dst_phys = (phys_bytes) dst_p.p_memmap[D].mem_phys << CLICK_SHIFT;
  dst_phys += dst_vir - (dst_p.p_memmap[D].mem_vir << CLICK_SHIFT);

  /* Make a physical copy for the requested data. */
  s = sys_abscopy(src_phys, dst_phys, bytes);

  return(s);
}

/*===========================================================================*
 *				    munmap	            		     *
 *===========================================================================*/
PUBLIC int munmap(void *addrstart, vir_bytes len)
{
  if(!unmap_ok) 
      return ENOSYS;

  return _munmap(addrstart, len);
}

/*===========================================================================*
 *			         munmap_text	            		     *
 *===========================================================================*/
PUBLIC int munmap_text(void *addrstart, vir_bytes len)
{
  if(!unmap_ok)
      return ENOSYS;

  return _munmap_text(addrstart, len);
}


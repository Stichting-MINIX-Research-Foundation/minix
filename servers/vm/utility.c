
/* This file contains some utility routines for VM.  */

#define _SYSTEM 1

#define _MINIX 1	/* To get the brk() prototype (as _brk()). */
#define brk _brk	/* Our brk() must redefine _brk(). */

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/type.h>
#include <string.h>
#include <errno.h>
#include <env.h>
#include <unistd.h>

#include "proto.h"
#include "glo.h"
#include "util.h"

#include <archconst.h>
#include <archtypes.h>
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"

/*===========================================================================*
 *                              get_mem_map                                  *
 *===========================================================================*/
PUBLIC int get_mem_map(proc_nr, mem_map)
int proc_nr;                                    /* process to get map of */
struct mem_map *mem_map;                        /* put memory map here */
{
	struct proc p;
	int s;

	if ((s=sys_getproc(&p, proc_nr)) != OK)
		return(s);

	memcpy(mem_map, p.p_memmap, sizeof(p.p_memmap));
	return(OK);
}

/*===========================================================================*
 *                              get_mem_chunks                               *
 *===========================================================================*/
PUBLIC void get_mem_chunks(mem_chunks)
struct memory *mem_chunks;                      /* store mem chunks here */ 
{  
/* Initialize the free memory list from the 'memory' boot variable.  Translate
 * the byte offsets and sizes in this list to clicks, properly truncated.
 */
  long base, size, limit;
  int i;
  struct memory *memp;

  /* Obtain and parse memory from system environment. */
  if(env_memory_parse(mem_chunks, NR_MEMS) != OK) 
        vm_panic("couldn't obtain memory chunks", NO_NUM); 
   
  /* Round physical memory to clicks. Round start up, round end down. */
  for (i = 0; i < NR_MEMS; i++) {
        memp = &mem_chunks[i];          /* next mem chunk is stored here */
        base = mem_chunks[i].base;
        size = mem_chunks[i].size;
        limit = base + size;
        base = (base + CLICK_SIZE-1) & ~(long)(CLICK_SIZE-1);
        limit &= ~(long)(CLICK_SIZE-1);
        if (limit <= base) {
                memp->base = memp->size = 0;
        } else { 
                memp->base = base >> CLICK_SHIFT;
                memp->size = (limit - base) >> CLICK_SHIFT;
        }
  }
}  

/*===========================================================================*
 *                              reserve_proc_mem                             *
 *===========================================================================*/
PUBLIC void reserve_proc_mem(mem_chunks, map_ptr)
struct memory *mem_chunks;                      /* store mem chunks here */
struct mem_map *map_ptr;                        /* memory to remove */
{
/* Remove server memory from the free memory list. The boot monitor
 * promises to put processes at the start of memory chunks. The 
 * tasks all use same base address, so only the first task changes
 * the memory lists. The servers and init have their own memory
 * spaces and their memory will be removed from the list.
 */
  struct memory *memp;
  for (memp = mem_chunks; memp < &mem_chunks[NR_MEMS]; memp++) {
        if (memp->base == map_ptr[T].mem_phys) {
                memp->base += map_ptr[T].mem_len + map_ptr[S].mem_vir;
                memp->size -= map_ptr[T].mem_len + map_ptr[S].mem_vir;
                break;
        }
  }
  if (memp >= &mem_chunks[NR_MEMS])
  {
        vm_panic("reserve_proc_mem: can't find map in mem_chunks ",
                map_ptr[T].mem_phys);
  }
} 

/*===========================================================================*
 *                              vm_isokendpt                           	     *
 *===========================================================================*/
PUBLIC int vm_isokendpt(endpoint_t endpoint, int *proc)
{
        *proc = _ENDPOINT_P(endpoint);
        if(*proc < -NR_TASKS || *proc >= NR_PROCS)
                return EINVAL;
        if(*proc >= 0 && endpoint != vmproc[*proc].vm_endpoint)
                return EDEADSRCDST;
        if(*proc >= 0 && !(vmproc[*proc].vm_flags & VMF_INUSE))
                return EDEADSRCDST;
        return OK;
}


struct proc mytmpproc;

/*===========================================================================*
 *                              get_stack_ptr                                *
 *===========================================================================*/
PUBLIC int get_stack_ptr(proc_nr_e, sp)
int proc_nr_e;                                  /* process to get sp of */   
vir_bytes *sp;                                  /* put stack pointer here */
{
  int s; 
  
  if ((s=sys_getproc(&mytmpproc, proc_nr_e)) != OK)     
        return(s);
  *sp = mytmpproc.p_reg.sp;
  return(OK);
}       

/*===========================================================================*
 *                              _brk                                         *
 *===========================================================================*/
extern char *_brksize;
PUBLIC int brk(brk_addr)
char *brk_addr;
{
        int r;
	struct vmproc *vmm = &vmproc[VM_PROC_NR];

/* VM wants to call brk() itself. */
        if((r=real_brk(vmm, (vir_bytes) brk_addr)) != OK)
		vm_panic("VM: brk() on myself failed\n", NO_NUM);
        _brksize = brk_addr;
        return 0;
}


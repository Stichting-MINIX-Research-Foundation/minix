#include "mm.h"
#include <minix/config.h>
#include <timers.h>
#include <string.h>
#include "../../kernel/const.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"

/* The entry points into this file are:
 *   p_getmap:	get memory map of given process
 *   p_getsp:	get stack pointer of given process	
 */

/*===========================================================================*
 *				p_getmap					     *
 *===========================================================================*/
PUBLIC int p_getmap(proc_nr, mem_map)
int proc_nr;					/* process to get map of */
struct mem_map *mem_map;			/* put memory map here */
{
  struct proc p;
  int s;

  if ((s=sys_getproc(&p, proc_nr)) != OK)
  	return(s);
  memcpy(mem_map, p.p_memmap, sizeof(p.p_memmap));
  return(OK);
}

/*===========================================================================*
 *				p_getsp					     *
 *===========================================================================*/
PUBLIC int p_getsp(proc_nr, sp)
int proc_nr;					/* process to get sp of */
vir_bytes *sp;					/* put stack pointer here */
{
  struct proc p;
  int s;

  if ((s=sys_getproc(&p, proc_nr)) != OK)
  	return(s);
  *sp = p.p_reg.sp;
  return(OK);
}


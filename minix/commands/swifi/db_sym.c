/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
#include "ddb.h"
#include "db_sym.h"
#include "swifi.h"

#include "extra.h"

/*
 * Multiple symbol tables
 */
#ifndef MAXNOSYMTABS
#define	MAXNOSYMTABS	3	/* mach, ux, emulator */
#endif

unsigned int	db_maxoff = 0x10000;
unsigned long modAddr = 0;

/* NWT: fault injection routine only. 
 * figure out start of function address given an address (off) in kernel text.
 * 	name = function name
 *	value = function address
 *  d = difference between off and function address
 * input is the desired address off and fault type
 * returns closest instruction address (if found), NULL otherwise
 */
unsigned long
find_faulty_instr(db_expr_t off, int type, int *instr_len)
{
  db_expr_t       d;
  char            *name;
  db_expr_t       value, cur_value, prev_value = 0;
  int		verbose=0, found=0;
  const char * mod_name = NULL;
  unsigned long mod_start;
  unsigned long mod_end;
  const char * sec_name = NULL;
  unsigned long sec_start;
  unsigned long sec_end;
  const char * sym_name = NULL;
  unsigned long sym_start;
  unsigned long sym_end;
  

  *instr_len = 0;
  if (kallsyms_address_to_symbol(off,
				 &mod_name, &mod_start, &mod_end,
				 &sec_name, &sec_start, &sec_end,
				 &sym_name, &sym_start, &sym_end) == 0) {
    return(0);
  }
  
  value = (db_expr_t) sym_start;
  d = off - sym_start;
  name = (char *) sym_name;

  if (name == 0) {
    value = off;
  }

  if (value >= DB_SMALL_VALUE_MIN && value <= DB_SMALL_VALUE_MAX) {
    printk("0x%x", off);
    return 0;
  }

  if (name == 0 || d >= db_maxoff) {
    printk("0x%x", off);
    return 0 ;
  }
  /* 2) backup to start of function (SOF) 
   * 3) delineate instruction boundaries, find instruction length too.
   */

  if(verbose) {
    printk("function %s", sym_name);
  }

  /* 4)  skip instructions until we get to our faulty address */
  cur_value = value;
  while(cur_value < sec_end) {
    if(verbose) { 
#if 0
      //	db_printsym(cur_value, DB_STGY_PROC); 
      //	printk(":\t");
#endif
    }
    prev_value=cur_value;
    modAddr=0;
    if(verbose) {
#if 0
      //cur_value=db_disasm(prev_value, FALSE); 
#endif
    } else {
      cur_value=my_disasm(prev_value, FALSE); 
    }
    
    /* 4a) bail out if instruction is leave (0xc9) */
    if(cur_value-prev_value == 1) {
      unsigned char *c;
      c=(unsigned char *) prev_value;
      if(text_read_ub(c)==0xc9)	{
	if(verbose) printk("bailing out as we hit a leave\n");
	found=0;
	break;
      }
    }
    /* 5a) init fault: from SOF, look for movl $X, -Y(%ebp), 
     *     (C645Fxxx or C745Fxxx) and replace with nop.
     */
    if(type==INIT_FAULT) {
      unsigned char *c;
      c=(unsigned char *) prev_value;

      if(*c==0x66 || *c==0x67) 
	c++;	/* override prefix */

      if(*c==0xC6 || *c==0xC7) 
	c++;	/* movb or movl imm */
      else 
	continue;

      if(*c==0x45) 
	c++;				/* [ebp]	*/
      else 
	continue;

      if(*c & 0x80) 
	found=1;			/* negative displacement */
      else 
	continue;

      found=1;	
      break;
    } else if(type==NOP_FAULT || type==STOP_FAULT) {
      /* 5b) nop*: replace instruction with nop */
      if(cur_value> off) {
	found=1;
	break;
      }
    } else if(type==DST_FAULT || type==SRC_FAULT) {
      /* 5c) dst/src: flip bits in mod/rm, sib, disp or imm fields */
      if(cur_value>off && (cur_value-prev_value) > 1) {
	found=1;
	break;
      }
    } else if(type==BRANCH_FAULT || type==LOOP_FAULT) {
      /* 5e) brc*: search forward utnil we hit a Jxx or rep (F3 or F2). 
       *     replace instr with nop.
       */
      unsigned char *c;

      c=(unsigned char *) prev_value;

      /* look for repX prefix */

      if(text_read_ub(c)==0xf3 || text_read_ub(c)==0xf2) {	
	if(verbose) 
	  printk("found repX prefix\n");
	/* take out repX prefix only */
	found=1;	
	cur_value=prev_value+1;		
	break;
      } else if( (text_read_ub(c)&0xf0)==0x70 ||
		(text_read_ub(c)>=0xe0 && text_read_ub(c)<=0xe2) ) {	
	/* look for jXX 8 (7X), loop,jcx (e0-3), jXX 16/32 (0f 8X) */
	found=1;	
	if(verbose) 
	  printk("found jXX rel8, loop or jcx\n");
	break;
      } else if(text_read_ub(c)==0x66 ||
		text_read_ub(c)==0x67)	{ 	/* override prefix */
	c++;
      } else if(text_read_ub(c++)==0xf && (text_read_ub(c)&0xf0)==0x80 ) {
	found=1;	/* 0x0f 0x8X */
	if(verbose) printk("found branch!\n");
	break;
      }
    } else if(type==PTR_FAULT) {
      /* 5f) ptr: if instruction has regmodrm byte (i_has_modrm), 
       *     and mod field has address ([eyy]dispxx), eyy!=ebp
       *     flip 1 bit in lower byte (0x0f) or any bit in following 
       *     bytes (sib, imm or disp).
       */
      if(cur_value>off && modAddr) {
	unsigned char *c;
	c=(unsigned char *) modAddr;
	if( text_read_ub(c)>0x3f && text_read_ub(c)<0xc0 &&
		(text_read_ub(c)&7)!=5 ) {
	  found=1;
	  break;
	}
      }
    } else if(type==INTERFACE_FAULT) {
      /* 5f) i/f: look for movl XX(ebp), reg or movb XX(ebp), reg,
       *     where XX is positive. replace instr with nop.
       *     movl=0x8a, movb=0x8b, mod=01XXX101 (disp8[ebp]), disp>0 
       */
      unsigned char *c;
      c=(unsigned char *) prev_value;
      if( text_read_ub(c)==0x8a || text_read_ub(c)==0x8b) {
	c++;
	if( ((text_read_ub(c++))&0xc7)==0x45 && (text_read_ub(c)&0x80)==0 ) {
	  /* 75% chance that we'll choose the next arg */
	  if(random()&0x3) {
	    found=1;
	    break;
	  } else {
	    if(verbose) printk("skipped...\n");
	  }
	}
      }
    }else if(type==IRQ_FAULT) {
      /* 5g) i/f: look for push reg or offset(reg) / popf,
       *     where XX is positive. replace instr with nop.
       *     movl=0x8a, movb=0x8b, mod=01XXX101 (disp8[ebp]), disp>0 
       */
      unsigned char *c;
      c=(unsigned char *) prev_value;
      if (((text_read_ub(c) & 0xf8) == 0x50) || 
	  (text_read_ub(c) == 0xff)) {
	if (text_read_ub(c) == 0xff) {
	  c++;
#if 0
	  //
	  // Look for push x(ebp)
#endif
	  if ((text_read_ub(c) & 0x78) != 0x70) {
	    continue;
	  }
	  /* 
	  // Skip the offset
	  */
	  c++;
	}
	c++;
	if (text_read_ub(c) == 0x9d) {
	  /*
	  // Increment cur_value to include the 
	  // popf instruction
	  */
	  cur_value++;
	  found = 1;
	  break;
	}
      }
      
    }
  }
  /* if we're doing nop fault, then we're done. 
   */
  if(found) {
    *instr_len=cur_value-prev_value;
    off=prev_value;
    if(verbose) {
      printk("%s", name);
      if (d) printk("+0x%x", d);
      printk(" @ %x, ", value);
      printk("instr @ %x, len=%d, ", off, *instr_len);
    }
    return off;
  } else {
    if(verbose) printk("cannot locate instruction in function\n"); 
    *instr_len=0;
    return 0;
  }
}

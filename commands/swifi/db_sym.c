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
#if 0
//#include <sys/param.h>
//#include <sys/systm.h>
#endif
#if 0
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/kallsyms.h>
#endif
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
#if 0

static db_symtab_t	db_symtabs[MAXNOSYMTABS] = {{0,},};
static int db_nsymtab = 0;

static db_symtab_t	*db_last_symtab;

static db_sym_t		db_lookup __P(( char *symstr));
static char		*db_qualify __P((db_sym_t sym, char *symtabname));
static boolean_t	db_symbol_is_ambiguous __P((db_sym_t sym));
static boolean_t	db_line_at_pc __P((db_sym_t, char **, int *, 
				db_expr_t));

/*
 * Add symbol table, with given name, to list of symbol tables.
 */
void
db_add_symbol_table(start, end, name, ref)
	char *start;
	char *end;
	char *name;
	char *ref;
{
	if (db_nsymtab >= MAXNOSYMTABS) {
		printk ("No slots left for %s symbol table", name);
		panic ("db_sym.c: db_add_symbol_table");
	}

	db_symtabs[db_nsymtab].start = start;
	db_symtabs[db_nsymtab].end = end;
	db_symtabs[db_nsymtab].name = name;
	db_symtabs[db_nsymtab].private = ref;
	db_nsymtab++;
}

/*
 *  db_qualify("vm_map", "ux") returns "unix:vm_map".
 *
 *  Note: return value points to static data whose content is
 *  overwritten by each call... but in practice this seems okay.
 */
static char *
db_qualify(sym, symtabname)
	db_sym_t	sym;
	register char	*symtabname;
{
	char		*symname;
	static char     tmp[256];

	db_symbol_values(sym, &symname, 0);
	strcpy(tmp,symtabname);
	strcat(tmp,":");
	strcat(tmp,symname);
	return tmp;
}


boolean_t
db_eqname(src, dst, c)
	char *src;
	char *dst;
	char c;
{
	if (!strcmp(src, dst))
	    return (TRUE);
	if (src[0] == c)
	    return (!strcmp(src+1,dst));
	return (FALSE);
}

boolean_t
db_value_of_name(name, valuep)
	char		*name;
	db_expr_t	*valuep;
{
	db_sym_t	sym;

	sym = db_lookup(name);
	if (sym == DB_SYM_NULL)
	    return (FALSE);
	db_symbol_values(sym, &name, valuep);
	return (TRUE);
}


/*
 * Lookup a symbol.
 * If the symbol has a qualifier (e.g., ux:vm_map),
 * then only the specified symbol table will be searched;
 * otherwise, all symbol tables will be searched.
 */
static db_sym_t
db_lookup(symstr)
	char *symstr;
{
	db_sym_t sp;
	register int i;
	int symtab_start = 0;
	int symtab_end = db_nsymtab;
	register char *cp;

	/*
	 * Look for, remove, and remember any symbol table specifier.
	 */
	for (cp = symstr; *cp; cp++) {
		if (*cp == ':') {
			*cp = '\0';
			for (i = 0; i < db_nsymtab; i++) {
				if (! strcmp(symstr, db_symtabs[i].name)) {
					symtab_start = i;
					symtab_end = i + 1;
					break;
				}
			}
			*cp = ':';
			if (i == db_nsymtab) {
				db_error("invalid symbol table name");
			}
			symstr = cp+1;
		}
	}

	/*
	 * Look in the specified set of symbol tables.
	 * Return on first match.
	 */
	for (i = symtab_start; i < symtab_end; i++) {
		sp = X_db_lookup(&db_symtabs[i], symstr);
		if (sp) {
			db_last_symtab = &db_symtabs[i];
			return sp;
		}
	}
	return 0;
}

/*
 * Does this symbol name appear in more than one symbol table?
 * Used by db_symbol_values to decide whether to qualify a symbol.
 */
static boolean_t db_qualify_ambiguous_names = FALSE;

static boolean_t
db_symbol_is_ambiguous(sym)
	db_sym_t	sym;
{
	char		*sym_name;
	register int	i;
	register
	boolean_t	found_once = FALSE;

	if (!db_qualify_ambiguous_names)
		return FALSE;

	db_symbol_values(sym, &sym_name, 0);
	for (i = 0; i < db_nsymtab; i++) {
		if (X_db_lookup(&db_symtabs[i], sym_name)) {
			if (found_once)
				return TRUE;
			found_once = TRUE;
		}
	}
	return FALSE;
}

/*
 * Find the closest symbol to val, and return its name
 * and the difference between val and the symbol found.
 */
db_sym_t
db_search_symbol( val, strategy, offp)
	register db_addr_t	val;
	db_strategy_t		strategy;
	db_expr_t		*offp;
{
	register
	unsigned int	diff;
	unsigned int	newdiff;
	register int	i;
	db_sym_t	ret = DB_SYM_NULL, sym;

	newdiff = diff = ~0;
	db_last_symtab = 0;
	for (i = 0; i < db_nsymtab; i++) {
	    sym = X_db_search_symbol(&db_symtabs[i], val, strategy, &newdiff);
	    if (newdiff < diff) {
		db_last_symtab = &db_symtabs[i];
		diff = newdiff;
		ret = sym;
	    }
	}
	*offp = diff;
	return ret;
}

/*
 * Return name and value of a symbol
 */
void
db_symbol_values(sym, namep, valuep)
	db_sym_t	sym;
	char		**namep;
	db_expr_t	*valuep;
{
	db_expr_t	value;

	if (sym == DB_SYM_NULL) {
		*namep = 0;
		return;
	}

	X_db_symbol_values(sym, namep, &value);
	if (db_symbol_is_ambiguous(sym))
		*namep = db_qualify(sym, db_last_symtab->name);
	if (valuep)
		*valuep = value;
}


/*
 * Print a the closest symbol to value
 *
 * After matching the symbol according to the given strategy
 * we print it in the name+offset format, provided the symbol's
 * value is close enough (eg smaller than db_maxoff).
 * We also attempt to print [filename:linenum] when applicable
 * (eg for procedure names).
 *
 * If we could not find a reasonable name+offset representation,
 * then we just print the value in hex.  Small values might get
 * bogus symbol associations, e.g. 3 might get some absolute
 * value like _INCLUDE_VERSION or something, therefore we do
 * not accept symbols whose value is "small" (and use plain hex).
 */


void
db_printsym(off, strategy)
	db_expr_t	off;
	db_strategy_t	strategy;
{
	db_expr_t	d;
	char 		*filename;
	char		*name;
	db_expr_t	value;
	int 		linenum;
	db_sym_t	cursym;

	cursym = db_search_symbol(off, strategy, &d);
	db_symbol_values(cursym, &name, &value);
	if (name == 0)
		value = off;
	if (value >= DB_SMALL_VALUE_MIN && value <= DB_SMALL_VALUE_MAX) {
		printk("0x%x", off);
		return;
	}
	if (name == 0 || d >= db_maxoff) {
		printk("0x%x", off);
		return;
	}
	printk("%s", name);
	if (d)
		printk("+0x%x", d);
	if (strategy == DB_STGY_PROC) {
	  //		if (db_line_at_pc(cursym, &filename, &linenum, off))
	  //			printk(" [%s:%d]", filename, linenum);
	}
}

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
    } else if(type==NOP_FAULT) {
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
#if 0
				// db_disasm(prev_value, FALSE); 
#endif
    }
    return off;
  } else {
    if(verbose) printk("cannot locate instruction in function\n"); 
    *instr_len=0;
    return 0;
  }
}

#if 0
static boolean_t
db_line_at_pc( sym, filename, linenum, pc)
	db_sym_t	sym;
	char		**filename;
	int		*linenum;
	db_expr_t	pc;
{
	return X_db_line_at_pc( db_last_symtab, sym, filename, linenum, pc);
}

int
db_sym_numargs(sym, nargp, argnames)
	db_sym_t	sym;
	int		*nargp;
	char		**argnames;
{
	return X_db_sym_numargs(db_last_symtab, sym, nargp, argnames);
}

#endif

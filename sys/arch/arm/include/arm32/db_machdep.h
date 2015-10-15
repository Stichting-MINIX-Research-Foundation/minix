/* $NetBSD: db_machdep.h,v 1.9 2014/03/28 21:54:12 matt Exp $ */

#ifndef _ARM32_DB_MACHDEP_H_
#define _ARM32_DB_MACHDEP_H_

#include <arm/db_machdep.h>

void db_show_frame_cmd(db_expr_t, bool, db_expr_t, const char *);
void db_show_fault_cmd(db_expr_t, bool, db_expr_t, const char *);
#ifdef _KERNEL
void db_show_tlb_cmd(db_expr_t, bool, db_expr_t, const char *);
#endif
#if defined(_KERNEL) && defined(MULTIPROCESSOR)
void db_switch_cpu_cmd(db_expr_t, bool, db_expr_t, const char *);
#endif

#endif

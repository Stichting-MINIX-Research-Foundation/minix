#ifndef _MAGIC_EVAL_H
#define _MAGIC_EVAL_H

#include <magic_def.h>

typedef long (*magic_eval_func_t) (long arg);

PUBLIC void magic_eval_init(void);

/* Eval frontends. */
PUBLIC int magic_eval_int(char *expr, long *result);
PUBLIC int magic_eval_bool(char *expr, char *result);
PUBLIC int magic_eval_float(char *expr, double *result);

/* Printing. */
#define MAGIC_EVAL_PRINT_FUNC_RESULTS   0x01
#define MAGIC_EVAL_PRINT_VAR_VALUES     0x02
#define MAGIC_EVAL_PRINT_STYLE_DEFAULT  0
#define MAGIC_EVAL_PRINT_STYLE_ALL      (MAGIC_EVAL_PRINT_FUNC_RESULTS|MAGIC_EVAL_PRINT_VAR_VALUES)

PUBLIC int magic_eval_get_print_style(void);
PUBLIC void magic_eval_set_print_style(int style);

#endif


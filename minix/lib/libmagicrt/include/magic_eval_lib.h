/* evaluate.h (C) 2000-2002 Kyzer/CSG. */
/* Released under the terms of the GNU General Public Licence version 2. */
/* http://www.kyzer.me.uk/code/evaluate/ */

#include <stddef.h>
#include <stdlib.h>

#define T_INT    0
#define T_REAL   1

/* value */
struct val {
  long   ival; /* if type = T_INT, this is the result */
  double rval; /* if type = T_REAL, this is the result */
  char   type; /* either T_INT or T_REAL */
};

/* variable */
struct var {
  struct var *next; /* next variable in table or NULL */
  struct val val;   /* value of variable */
  char   *name;     /* name of variable */
};

/* variable table */
struct vartable {
  struct var *first; /* first entry in variable table */
  struct memh *mh;
};

/* creates a new variable table (NULL if no memory) */
struct vartable *create_vartable(void);

/* frees a variable table */
void free_vartable(struct vartable *vt);

/* gets a variable from a variable table (NULL if not found) */
struct var *get_var(struct vartable *vt, char *name);

/* puts a variable into a variable table (NULL if no memory) */
struct var *put_var(struct vartable *vt, char *name, struct val *value);

/* callbacks */
typedef struct val*(*get_var_cb_t)(char*, struct val*);
typedef struct val*(*get_func_result_cb_t)(char*, struct val*, struct val*);
void eval_set_cb_get_var(get_var_cb_t cb);
void eval_set_cb_get_func_result(get_func_result_cb_t cb);

/* THE FUNCTION YOU WANT TO CALL */

/* given a string to evaluate (not NULL), a result to put the answer in
 * (not NULL) and optionally your own variable table (NULL for 'internal
 * only' vartable), will return an error code (and result, etc)
 */
int evaluate(char *eval, struct val *result, struct vartable *variables);

/* errors */
#define RESULT_OK               0       /* all OK                       */
#define ERROR_SYNTAX            2       /* invalid expression           */
#define ERROR_VARNOTFOUND       3       /* variable not found           */
#define ERROR_FUNCNOTFOUND      4       /* function not found           */
#define ERROR_NOMEM             8       /* not enough memory available  */
#define ERROR_DIV0              9       /* division by zero             */
#define ERROR_BUSY             10       /* busy now                     */

/* configuration */
#define TOKEN_DEBUG             0
#define EVAL_DEBUG              0
#define EVAL_MALLOC             0
#define USE_MATH_LIB            0
#define MEM_DEBUG               0
#define MEM_LOW_FOOTPRINT       1
#define VAR_FROM_ENV            0


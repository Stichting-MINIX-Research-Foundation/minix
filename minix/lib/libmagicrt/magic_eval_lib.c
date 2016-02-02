/* evaluate.c (C) 2000-2002 Kyzer/CSG. */
/* Released under the terms of the GNU General Public Licence version 2. */

#include "magic_eval_lib.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#if USE_MATH_LIB
#include <math.h>
#endif

#ifdef __MINIX
/* FIXME: due to the current linker command line ordering, parts of lib(min)c
 * that are used exclusively by libmagicrt end up not being instrumented, which
 * then causes problems transferring pointers such as _ctype_tab_ and
 * _tolower_tab_. As a temporary workaround, we redefine the macros that use
 * those pointers. This code is currently never triggered so it is not
 * performance critical; obviously there are a million better ways to do this.
 */
#undef isalpha
#define isalpha(c) ((unsigned)(((c) & ~0x20) - 'A') <= ('Z' - 'A'))
#undef isupper
#define isupper(c) ((unsigned)((c) - 'A') <= ('Z' - 'A'))
#undef islower
#define islower(c) ((unsigned)((c) - 'a') <= ('z' - 'a'))
#undef isdigit
#define isdigit(c) ((unsigned)((c) - '0') <= ('9' - '0'))
static inline int __isxdigit(c) {
    return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
#undef isxdigit
#define isxdigit(c) (__isxdigit(c))
static inline int __isspace(c) {
    switch (c) {
    case ' ': case '\t': case '\n': case '\v': case '\f': case '\r': return 1;
    default: return 0;
    }
}
#undef isspace
#define isspace(c) (__isspace(c))
static inline int __tolower(c) {
    return isupper(c) ? (c | 0x20) : c;
}
#undef tolower
#define tolower(c) (__tolower(c))
#endif /* __MINIX */

/* a token structure */
struct tok {
    struct tok *next;
    struct var *var;
    char     token;
    struct val val;
    char     funcid, *name, *name_end;
};
#define REG_TOK_SIZE() offsetof(struct tok, val)
#define VAL_TOK_SIZE() offsetof(struct tok, funcid)
#define VAR_TOK_SIZE() sizeof(struct tok)

/* private memory header for tracked memory allocation */
struct memh {
    struct memh *next;
    void *ptr;
};

/* creates a new memory header for allocating memory */
static struct memh *create_mem();

/* allocates memory using a particular header */
static void *mem_alloc(struct memh *mh, size_t len);

/* frees all memory for a particular header */
static void free_mem(struct memh *mh);

/* token types */
enum {
    /* parentheses */
    TK_OPEN, TK_CLOSE,

    /* variables and values */
    TK_VAR, TK_VARQUOTE, TK_VAL,

    /* binary operators */
    TK_ADD, TK_SUB, TK_MUL, TK_MULI, TK_DIV,
    TK_MOD, TK_POW, TK_AND, TK_OR, TK_BAND,
    TK_BOR, TK_BXOR, TK_EQ, TK_NE, TK_LT, TK_GT,
    TK_LE, TK_GE, TK_SHL, TK_SHR,

    /* unary operators */
    TK_ASSN, TK_NEG, TK_FUNC, TK_NOT, TK_BNOT,

    /* special scan codes */
    TK_BREAK, /* finish scanning, bring remainder of string forward */
    TK_ERROR, /* abort scanning */
    TK_SKIP     /* ignore the character */
};

/* lookup table to do conversion [char -> token type] */
char scantable[UCHAR_MAX+1];
int scantable_ok = 0;

/* table of function names */
const char *functable[] = {
    "acos", "asin", "atan", "cos", "cosh", "exp", "ln", "log",
    "sin", "sinh", "sqr", "sqrt", "tan", "tanh", NULL
};

/* function ids (index to functable) */
enum {
    F_ACOS, F_ASIN, F_ATAN, F_COS, F_COSH, F_EXP, F_LN, F_LOG,
    F_SIN, F_SINH, F_SQR, F_SQRT, F_TAN, F_TANH
};

/* callbacks */
static get_var_cb_t get_var_cb = NULL;
static get_func_result_cb_t get_func_result_cb = NULL;
void eval_set_cb_get_var(get_var_cb_t cb) {
        get_var_cb = cb;
}
void eval_set_cb_get_func_result(get_func_result_cb_t cb) {
        get_func_result_cb = cb;
}

int same_str(const char *a, const char *b);
int same_str_len(const char *a, const char *b, int len);

void init_scantable(void);
int tokenize(struct memh *mh, char **string, struct tok **listptr);
int scan_number(char **stringptr, struct val *valptr);
int precedence(struct tok *t);
int eval(struct memh *mh, struct tok *list, struct vartable *vt,
    struct val *result);
void prt_lst(struct tok *t);
void prt_tok(struct tok *t);

/*** FRONT-END ***/

int evaluate(char *expr, struct val *result, struct vartable *vartable) {
    struct memh *mh = NULL;
    int error = RESULT_OK, madevar = 0;
    struct tok *list;
    char *str;

    /* ensure we have a variable table */
    if (!vartable) madevar = 1, vartable = create_vartable();
    if (!vartable) return ERROR_NOMEM;

    init_scantable();
    result->type = T_INT;
    result->ival = 0;

    if ((mh = create_mem())) {
        if (expr && (str = (char *) mem_alloc(mh, strlen(expr)+1))) {
            strcpy(str, expr);
            while (*str) {
    if ((error = tokenize(mh, &str, &list)) != RESULT_OK) break;
    if ((error = eval(mh, list, vartable, result)) != RESULT_OK) break;
            }
        } else error = ERROR_NOMEM;
    } else error = ERROR_NOMEM;

    if(mh) free_mem(mh);
    if (madevar) free_vartable(vartable);
    return error;
}




/**** TOKENIZATION ***/

void init_scantable(void) {
    int i;

    if (scantable_ok) return;

    for (i = 0; i <= UCHAR_MAX; i++)
        scantable[i] =
            (isalpha(i) || i == '_') ? TK_VAR :
         (isdigit(i) ? TK_VAL :
         (isspace(i) ? TK_SKIP :
                                     TK_ERROR));

    scantable['+'] = TK_ADD;
    scantable['-'] = TK_SUB;
    scantable['*'] = TK_MUL;    /* also '**' = TK_POW */
    scantable['/'] = TK_DIV;
    scantable['%'] = TK_MOD;
    scantable['$'] = TK_VAL;    /* '$' starts a hexadecimal value */
    scantable['.'] = TK_VAL;    /* '.' starts a fractional value */
    scantable['('] = TK_OPEN;
    scantable[')'] = TK_CLOSE;
    scantable[';'] = TK_BREAK;
    scantable['='] = TK_ASSN; /* also '==' = TK_EQ */
    scantable['~'] = TK_BNOT;
    scantable['^'] = TK_BXOR;
    scantable['&'] = TK_BAND; /* also '&&' = TK_AND */
    scantable['|'] = TK_BOR;    /* also '||' = TK_OR */
    scantable['!'] = TK_NOT;    /* also '!=' = TK_NE */
    scantable['<'] = TK_LT;     /* also '<<' = TK_SHL, '<=' = TK_LE */
    scantable['>'] = TK_GT;     /* also '>>' = TK_SHR, '>=' = TK_GE */
    scantable['\''] = TK_VARQUOTE;

    scantable_ok = 1;
}

#if !MEM_LOW_FOOTPRINT

int tokenize(struct memh *mh, char **string, struct tok **listptr) {
    struct tok *list;
    int idx = 0, i, len;
    char *s, *name, c, c2, nt;

    /* allocate a block of memory to hold the maximum amount of tokens */
    i = strlen(*string) + 1;
    list = (struct tok *) mem_alloc(mh, i * sizeof(struct tok));
    if (!list) return ERROR_NOMEM;

    for (s = *string; *s; s++) {
        /* get token type of character and store into list */
        c = list[idx].token = scantable[* (unsigned char *) s];

#if TOKEN_DEBUG
         printf("tokenize: token %p code %d string %s\n", &list[idx], list[idx].token, s);
#endif

        /* break out of the for loop on TK_BREAK */
        if (c == TK_BREAK) { s++; break; }

        switch (c) {
        case TK_ERROR:
            return ERROR_SYNTAX;

        case TK_SKIP:
            break;

        /* most symbol-tokens fall under this one - nothing much to do */
        case TK_OPEN: case TK_CLOSE: case TK_ADD: case TK_SUB:
        case TK_MUL: case TK_DIV: case TK_MOD: case TK_BAND: case TK_BOR:
        case TK_BXOR: case TK_BNOT: case TK_NOT: case TK_LT: case TK_GT:

            /* check for 'double character' tokens */
            c2 = s[1];
            nt = 0;
            if (c == TK_MUL  && c2 == '*') nt = TK_POW;
            if (c == TK_BAND && c2 == '&') nt = TK_AND;
            if (c == TK_BOR  && c2 == '|') nt = TK_OR;
            if (c == TK_NOT  && c2 == '=') nt = TK_NE;
            if (c == TK_LT   && c2 == '=') nt = TK_LE;
            if (c == TK_LT   && c2 == '<') nt = TK_SHL;
            if (c == TK_GT   && c2 == '=') nt = TK_GE;
            if (c == TK_GT   && c2 == '>') nt = TK_SHR;
            if (nt) { list[idx].token = nt; s++; }

            idx++;
            break;

        case TK_ASSN:
            /* '=' = TK_ASSN, '==' = TK_EQ */
            if (s[1] == '=') { list[idx++].token = TK_EQ; s++; break; }

            /* if the last token was a variable, change it to an assignment */
            if (idx <= 0 || list[idx-1].token != TK_VAR) return ERROR_SYNTAX;
            list[idx-1].token = TK_ASSN;
            break;

        case TK_VAL:
            if (!scan_number(&s, &list[idx++].val)) return ERROR_SYNTAX;
            s--; /* wind back one for the loop's iterator */
            break;

        case TK_VAR:
        case TK_VARQUOTE:
            if(c == TK_VAR) {
                    list[idx].name = name = s;
                    c2 = scantable[(int)s[1]];
                    while (c2 == TK_VAR || c2 == TK_VAL) {
                            s++; /* skip to end of string */
                            c2 = scantable[(int)s[1]];
                    }
            }
            else {
                    if(!s[1] || scantable[(int)s[1]] == TK_VARQUOTE) {
                            return ERROR_SYNTAX;
                    }
                    list[idx].token = TK_VAR;
                    list[idx].name = name = ++s;
                    while (s[1] && scantable[(int)s[1]] != TK_VARQUOTE) s++; /* skip to end of string */
                    if(!s[1]) {
                            return ERROR_SYNTAX;
                    }
            }
            list[idx].name_end = s+1;
            len = s+1 - name;
            if(c == TK_VARQUOTE) {
                    s++;
            }

            if(scantable[(int)s[1]] == TK_OPEN) {
                    list[idx].token  = TK_FUNC;
                    list[idx].funcid = 0;
                    /* look for matching function */
                    for (i = 0; functable[i]; i++) {
                        char *fname = functable[i];
                        if (same_str_len(name, fname, len) && strlen(fname) == len) {
                            list[idx].funcid = i+1;
                            break;
                        }
                    }
            }
            idx++;
            break;
        }
    }

    /* write back the final position of the tokenizer - either pointing at
     * a null character, or the next expression to go */
    *string = s;

    /* lace up the tokens and null-terminate the strings */
    if (idx > 0) {
        for (i = 0; i < idx; i++) {
            list[i].next = &list[i+1];
#if TOKEN_DEBUG
            printf("tokenize: processed token %p code %d\n", &list[i], list[i].token);
#endif
            if (list[i].token == TK_VAR || list[i].token == TK_ASSN || list[i].token == TK_FUNC)
                *(list[i].name_end) = '\0';
        }
        list[idx-1].next = NULL;
        *listptr = list;
    }
    else {
        *listptr = NULL;
    }

    return RESULT_OK;
}

#else

int tokenize(struct memh *mh, char **string, struct tok **listptr) {
    struct tok *tok = NULL, *last_tok = NULL;
    int idx = 0, i, len;
    char *s, *name, c, c2, nt;
    int skip_alloc = 0;

    for (s = *string; *s; s++) {
        /* get token type of character and store into list */
        c = scantable[* (unsigned char *) s];

        /* break out of the for loop on TK_BREAK */
        if(c == TK_BREAK) { s++; break; }
        if(c == TK_ERROR) return ERROR_SYNTAX;

        if(c == TK_SKIP) continue;

        if(!skip_alloc) {
                size_t tok_size;
                last_tok = tok;
                switch(c) {
                        case TK_VAL:
                                tok_size = VAL_TOK_SIZE();
                        break;
                        case TK_VAR:
                        case TK_VARQUOTE:
                        case TK_ASSN:
                                tok_size = VAR_TOK_SIZE();
                        break;
                        default:
                                tok_size = REG_TOK_SIZE();
                        break;
                }
                tok = (struct tok *) mem_alloc(mh, tok_size);
                if(!tok) return ERROR_NOMEM;
                tok->next = NULL;
                if(last_tok) {
                        last_tok->next = tok;
                }
                else {
                        *listptr = tok;
                }
        }
        else {
                skip_alloc = 0;
        }
        tok->token = c;

#if TOKEN_DEBUG
        printf("tokenize: token %p code %d string %s\n", tok, tok->token, s);
#endif

        switch (c) {

        /* most symbol-tokens fall under this one - nothing much to do */
        case TK_OPEN: case TK_CLOSE: case TK_ADD: case TK_SUB:
        case TK_MUL: case TK_DIV: case TK_MOD: case TK_BAND: case TK_BOR:
        case TK_BXOR: case TK_BNOT: case TK_NOT: case TK_LT: case TK_GT:

            /* check for 'double character' tokens */
            c2 = s[1];
            nt = 0;
            if (c == TK_MUL  && c2 == '*') nt = TK_POW;
            if (c == TK_BAND && c2 == '&') nt = TK_AND;
            if (c == TK_BOR  && c2 == '|') nt = TK_OR;
            if (c == TK_NOT  && c2 == '=') nt = TK_NE;
            if (c == TK_LT   && c2 == '=') nt = TK_LE;
            if (c == TK_LT   && c2 == '<') nt = TK_SHL;
            if (c == TK_GT   && c2 == '=') nt = TK_GE;
            if (c == TK_GT   && c2 == '>') nt = TK_SHR;
            if (nt) { tok->token = nt; s++; }

            idx++;
            break;

        case TK_ASSN:
            /* '=' = TK_ASSN, '==' = TK_EQ */
            if (s[1] == '=') { tok->token = TK_EQ; idx++; s++; break; }

            /* if the last token was a variable, change it to an assignment */
            if (idx <= 0 || last_tok->token != TK_VAR) return ERROR_SYNTAX;
            last_tok->token = TK_ASSN;
            skip_alloc = 1;
            break;

        case TK_VAL:
            if (!scan_number(&s, &tok->val)) return ERROR_SYNTAX;
            idx++;
            s--; /* wind back one for the loop's iterator */
            break;

        case TK_VAR:
        case TK_VARQUOTE:
            if(c == TK_VAR) {
                    tok->name = name = s;
                    c2 = scantable[(int)s[1]];
                    while (c2 == TK_VAR || c2 == TK_VAL) {
                            s++; /* skip to end of string */
                            c2 = scantable[(int)s[1]];
                    }
            }
            else {
                    if(!s[1] || scantable[(int)s[1]] == TK_VARQUOTE) {
                            return ERROR_SYNTAX;
                    }
                    tok->token = TK_VAR;
                    tok->name = name = ++s;
                    while (s[1] && scantable[(int)s[1]] != TK_VARQUOTE) s++; /* skip to end of string */
                    if(!s[1]) {
                            return ERROR_SYNTAX;
                    }
            }
            tok->name_end = s+1;
            len = s+1 - name;
            if(c == TK_VARQUOTE) {
                    s++;
            }

            if(scantable[(int)s[1]] == TK_OPEN) {
                    tok->token  = TK_FUNC;
                    tok->funcid = 0;
                    /* look for matching function */
                    for (i = 0; functable[i]; i++) {
                        const char *fname = functable[i];
                        if (same_str_len(name, fname, len) && strlen(fname) == (size_t)len) {
                            tok->funcid = i+1;
                            break;
                        }
                    }
            }
            idx++;
            break;
        }
    }

    /* write back the final position of the tokenizer - either pointing at
     * a null character, or the next expression to go */
    *string = s;

    /* lace up the tokens and null-terminate the strings */
    if (idx > 0) {
        tok = *listptr;
        do {
#if TOKEN_DEBUG
            printf("tokenize: processed token %p code %d\n", tok, tok->token);
#endif
            if (tok->token == TK_VAR || tok->token == TK_ASSN || tok->token == TK_FUNC)
                *(tok->name_end) = '\0';
            tok = tok->next;
        } while(tok);
    }
    else {
        *listptr = NULL;
    }

    return RESULT_OK;
}

#endif

/* scans some text into a value */
int scan_number(char **stringptr, struct val *valptr) {
    struct val v = { T_INT, 0, 0.0 };
    char *s = *stringptr;
    int c;
    double dp;

    /* test to see if it's a hex number */
    if (s[0] == '$' || (s[0] == '0' && s[1] == 'x')) {
        s += (s[1] == 'x') ? 2 : 1;
        *stringptr = s;

        for (; isxdigit(c = (int) *s); s++)
            v.ival = (v.ival << 4)
                + (isdigit(c) ? c-'0'            : 0)
                + (isupper(c) ? c-'A' + 10 : 0)
                + (islower(c) ? c-'a' + 10 : 0);
    }

    /* must be a decimal integer or real */
    else {
        for (; isdigit(c = (int) *s); s++) v.ival = (v.ival * 10) + c-'0';
        if (*s == '.') {
            *stringptr = ++s;
            v.type = T_REAL;
            v.rval = (double) v.ival;
            for (dp = 0.1; isdigit(c = (int) *s); s++, dp /= 10.0)
                v.rval += dp * (double) (c-'0');
        }
    }

    /* if no numeric chars have been read, it's a dud - return FAIL */
    if (s == *stringptr) return 0;

    /* otherwise, update position and return SUCCESS */
    *stringptr = s;
    *valptr = v;
    return 1;
}


/*** EVALUATION ***/

/* returns the precedence of a token */
int precedence(struct tok *t) {
    switch (t->token) {
    case TK_FUNC:                                                                                                   return 15;
    case TK_MULI:                           return 14;
    case TK_NEG:    case TK_NOT:    case TK_BNOT:           return 13;
    case TK_POW:                            return 12;
    case TK_MUL:    case TK_DIV:    case TK_MOD:            return 11;
    case TK_ADD:    case TK_SUB:                    return 10;
    case TK_SHL:    case TK_SHR:                    return 9;
    case TK_LT: case TK_GT: case TK_LE: case TK_GE: return 8;
    case TK_EQ: case TK_NE:                 return 7;
    case TK_BAND:                           return 6;
    case TK_BOR:    case TK_BXOR:                   return 5;
    case TK_AND:    case TK_OR:                 return 4;
    case TK_ASSN:                           return 3;
    case TK_CLOSE:                                  return 2;
    case TK_OPEN:                           return 1;
    }
    return 0;
}


int eval(struct memh *mh, struct tok *list, struct vartable *vt,
    struct val *result) {

    static struct val newval = { T_INT, 0, 0.0 };

    struct val tmp_val, *valstk, *x, *y;
    struct tok open, close, *l, *r, *t, **opstk;
    char lt, rt, token;
    int vstk, ostk, vcnt = 0, ocnt = 0;
    double xr, yr, rr = 0;
    long xi, yi, ri = 0;
#if VAR_FROM_ENV
    char *envtxt;
#endif

    /* clear result before we do anything - and no tokens is no result */
    *result = newval;
    if (!list) return RESULT_OK;


    /* CONVERSION OF RAW TOKENS INTO COMPLETE INFIX EXPRESSION */

    /* wrap the token list in a pair of parentheses */
    for (t = list; t->next; t = t->next)
            ;
    t->next = &close;
    close.next = NULL; open.next = list; list = &open;
    close.token = TK_CLOSE; open.token  = TK_OPEN;

    /* insert and change tokens as neccessary */
    for (l=list, r=l->next; r->next; l=r, r=r->next) {
        lt = l->token;
        rt = r->token;

        /* convert TK_SUBs that should be unary into TK_NEGs */
        if (rt == TK_SUB && lt != TK_CLOSE && lt != TK_VAR && lt != TK_VAL)
            r->token = TK_NEG;

        /* insert implicit multiplication tokens */
        if ((lt == TK_VAR || lt == TK_VAL || lt == TK_CLOSE)
        && (rt == TK_VAR || rt == TK_VAL || rt == TK_OPEN || rt == TK_FUNC)) {
            if (lt == rt) return ERROR_SYNTAX;
            t = (struct tok *) mem_alloc(mh, sizeof(struct tok));
            if (!t) return ERROR_NOMEM;
            t->token = TK_MULI; l->next = t; t->next = r;
        }
    }

    /* VARIABLE CHECKING */

    vcnt = ocnt = 0;
    for (t = list; t; t = t->next) {
        lt = t->token;

        /* count the number of values and operators */
        if (lt == TK_VAR || lt == TK_VAL) vcnt++; else ocnt++;

        /* if assigned variables don't exist, create a new blank one */
        if (lt == TK_ASSN) {
            if (!(t->var = get_var(vt, t->name)))
                if (!(t->var = put_var(vt, t->name, &newval)))
                    return ERROR_NOMEM;
        }

        /* try to get vars from vartable - if not, try the environment */
        else if (lt == TK_VAR) {
            int var_found = 0;
            if ((t->var = get_var(vt, t->name))) {
                    var_found = 1;
            }
#if VAR_FROM_ENV
            if(!var_found && (envtxt = getenv(t->name))) {
                    if (!scan_number(&envtxt, &tmp_val)) return ERROR_SYNTAX;
                    if (!(t->var = put_var(vt, t->name, &tmp_val))) return ERROR_NOMEM;
                    var_found = 1;
            }
#endif
            if(!var_found && get_var_cb && (get_var_cb(t->name, &tmp_val))) {
                    if (!(t->var = put_var(vt, t->name, &tmp_val))) return ERROR_NOMEM;
                    var_found = 1;
            }
            if(!var_found) {
                    return ERROR_VARNOTFOUND;
            }
        }
    }

    /* ALLOCATE STACKS */

    /* allocate the operator stack and the value stack */
    valstk = (struct val *)  mem_alloc(mh, vcnt * sizeof(struct val));
    opstk  = (struct tok **) mem_alloc(mh, ocnt * sizeof(struct tok *));
    if (!valstk || !opstk) return ERROR_NOMEM;

    /* set the stack pointers to '0 items on stack' */
    /* (the stack pointers are always set at the topmost stack item) */
    ostk = vstk = -1;

    /* MAIN EVALUATION LOOP */
    prt_lst(list);
    for (t = list; t; t=t->next) {
        switch (t->token) {

        /* unary operators always wait until after what follows is evaluated */
        /* also, open parentheses are pushed to match where close ones stop */
        case TK_OPEN:
        case TK_ASSN: case TK_NEG: case TK_FUNC: case TK_NOT: case TK_BNOT:
            opstk[++ostk] = t; break;

        /* values go straight on the value stack */
        case TK_VAL:
            valstk[++vstk] = t->val;
            break;

        /* variables go straight on the value stack */
        case TK_VAR:
            valstk[++vstk] = t->var->val;
            break;

        /* this is where the action happens - all operations of a higher or same
         * precedence are now executed. then, after that, we push the operator
         * to the stack, or if it's a close paren, pull and expect an open paren
         *
         * it's assumed that all tokens in the token stream that aren't one of
         * the previous cases must be the close bracket or a binary operator -
         * that's why 'default' is used rather than all the names
         */
        default:
            while (precedence(opstk[ostk]) >= precedence(t)) {
                struct tok *op = opstk[ostk--];
#if EVAL_DEBUG
                printf("eval: "); prt_tok(op); printf("\n");
#endif

                /* there should always be at least a close bracket left here */
                if (ostk < 0) return ERROR_SYNTAX;

                /* we assume that all operators require at least one value */
                /* on the stack, and check here */
                if (vstk < 0) return ERROR_SYNTAX;

                /* now we actually perform evaluations */
                switch (token = op->token) {

                /* binary (int/real) -> (int/real) */
                case TK_ADD: case TK_SUB: case TK_MUL: case TK_MULI:

                    /* pull two values from the stack, y then x, and push 'x op y' */
                    if (vstk < 1) return ERROR_SYNTAX;
                    y = &valstk[vstk--]; x = &valstk[vstk];

                    /* if both values are integer, do integer operations only */
                    if (x->type == T_INT && y->type == T_INT) {
                        xi = x->ival;
                        yi = y->ival;
                        switch (token) {
                        case TK_MULI:
                        case TK_MUL: ri = (xi * yi); break;
                        case TK_ADD: ri = (xi + yi); break;
                        case TK_SUB: ri = (xi - yi); break;
                        }
                        /* push int-value result to value stack */
                        x->type = T_INT;
                        x->ival = ri;
                    }
                    else {
                        /* get real values - convert if neccessary */
                        xr = (x->type == T_REAL) ? x->rval : (double) x->ival;
                        yr = (y->type == T_REAL) ? y->rval : (double) y->ival;

                        switch (token) {
                        case TK_MULI:
                        case TK_MUL: rr = (xr * yr); break;
                        case TK_ADD: rr = (xr + yr); break;
                        case TK_SUB: rr = (xr - yr); break;
                        }
                        /* push real-value result to value stack */
                        x->type = T_REAL;
                        x->rval = rr;
                    }
                    break;



                /* binary (int/real) -> int */
                case TK_EQ: case TK_NE: case TK_LT:
                case TK_GT: case TK_LE: case TK_GE:

                    if (vstk < 1) return ERROR_SYNTAX;
                    y = &valstk[vstk--]; x = &valstk[vstk];

                    if (x->type == T_INT && y->type == T_INT) {
                        xi = x->ival;
                        yi = y->ival;
                        switch (token) {
                        case TK_EQ:  ri = (xi == yi); break;
                        case TK_NE:  ri = (xi != yi); break;
                        case TK_LT:  ri = (xi <  yi); break;
                        case TK_GT:  ri = (xi >  yi); break;
                        case TK_LE:  ri = (xi <= yi); break;
                        case TK_GE:  ri = (xi >= yi); break;
                        }
                    }
                    else {
                        xr = (x->type == T_REAL) ? x->rval : (double) x->ival;
                        yr = (y->type == T_REAL) ? y->rval : (double) y->ival;
                        switch (token) {
                        case TK_EQ:  ri = (xr == yr); break;
                        case TK_NE:  ri = (xr != yr); break;
                        case TK_LT:  ri = (xr <  yr); break;
                        case TK_GT:  ri = (xr >  yr); break;
                        case TK_LE:  ri = (xr <= yr); break;
                        case TK_GE:  ri = (xr >= yr); break;
                        }
                    }
                    x->type = T_INT;
                    x->ival = ri;
                    break;


                /* binary real -> real */
                case TK_DIV: case TK_POW:

                    if (vstk < 1) return ERROR_SYNTAX;
                    y = &valstk[vstk--]; x = &valstk[vstk];
                    xr = (x->type == T_REAL) ? x->rval : (double) x->ival;
                    yr = (y->type == T_REAL) ? y->rval : (double) y->ival;

                    if (token == TK_DIV) {
                        if (yr == 0) return ERROR_DIV0;
                        x->rval = xr / yr;
                    }
                    else {
#if USE_MATH_LIB
                        x->rval = pow(xr, yr);
#else
                        x->rval = 0;
#endif
                    }
                    x->type = T_REAL;
        break;


                /* binary int -> int */
                case TK_MOD: case TK_AND: case TK_OR:
                case TK_BAND: case TK_BOR: case TK_BXOR:
                case TK_SHL: case TK_SHR:

                    if (vstk < 1) return ERROR_SYNTAX;
                    y = &valstk[vstk--]; x = &valstk[vstk];
                    xi = (x->type == T_INT) ? x->ival : (long) x->rval;
                    yi = (y->type == T_INT) ? y->ival : (long) y->rval;

                    switch (token) {
                    case TK_MOD:    if (yi == 0) return ERROR_DIV0;
                                                ri = (xi %  yi); break;
                    case TK_AND:    ri = (xi && yi); break;
                    case TK_OR:     ri = (xi || yi); break;
                    case TK_BAND: ri = (xi &    yi); break;
                    case TK_BOR:    ri = (xi |  yi); break;
                    case TK_BXOR: ri = (xi ^    yi); break;
                    case TK_SHL:    ri = (xi << yi); break;
                    case TK_SHR:    ri = (xi >> yi); break;
                    }

                    x->type = T_INT;
                    x->ival = ri;
                    break;



                /* unary real -> real */
                case TK_FUNC:
                    x = &valstk[vstk];
                    if(op->funcid) {
#if USE_MATH_LIB
                            xr = (x->type == T_REAL) ? x->rval : (double) x->ival;
                            switch (op->funcid-1) {
                            case F_ACOS: xr =  acos(xr); break;
                            case F_ASIN: xr =  asin(xr); break;
                            case F_ATAN: xr =  atan(xr); break;
                            case F_COS:  xr =       cos(xr); break;
                            case F_COSH: xr =  cosh(xr); break;
                            case F_EXP:  xr =       exp(xr); break;
                            case F_LN:   xr =       log(xr); break;
                            case F_LOG:  xr = log10(xr); break;
                            case F_SIN:  xr =       sin(xr); break;
                            case F_SINH: xr =  sinh(xr); break;
                            case F_SQR:  xr =       xr * xr; break;
                            case F_SQRT: xr =  sqrt(xr); break;
                            case F_TAN:  xr =       tan(xr); break;
                            case F_TANH: xr =  tanh(xr); break;
                            default:         xr = 0;                 break;
                            }
#else
                            xr = 0;
#endif
                            x->rval = xr;
                            x->type = T_REAL;
                    }
                    else {
                            if(!get_func_result_cb || !get_func_result_cb(op->name, x, x)) {
                                    return ERROR_FUNCNOTFOUND;
                            }
                    }
                    break;


                /* unary int -> int */
                case TK_BNOT: case TK_NOT:

                    x = &valstk[vstk];
                    xi = (x->type == T_INT) ? x->ival : (long) x->rval;
                    if (token == TK_BNOT) {
                        x->ival = ~ xi;
                    }
                    else {
                        x->ival = ! xi;
                    }
                    x->type = T_INT;
                    break;


                /* unary (int/real) -> (int/real) */
                case TK_ASSN:
                    op->var->val = valstk[vstk];
                    break;


                /* unary (int/real) -> (int/real) */
                case TK_NEG:
                    x = &valstk[vstk];
                    if (x->type == T_INT)
                        x->ival = - x->ival;
                    else
                        x->rval = - x->rval;
                    break;

                } /* end select (execution switch) */
            } /* end while (precedence loop) */

            /* back to the postfixified */

            /* if we had a close paren, pull the matching open paren (error if
             * we pull something else. otherwise push our new operator
             */
            if (t->token == TK_CLOSE) {
                if (opstk[ostk--]->token != TK_OPEN) return ERROR_SYNTAX;
            }
            else {
                opstk[++ostk] = t;
            }
        }
    }

    /* there should be exactly one value and no operators left on the stacks */
    if (vstk != 0 || ostk != -1) return ERROR_SYNTAX;

    /* return that value */
    *result = valstk[0];
    return RESULT_OK;
}





/** debugging things **/
#if EVAL_DEBUG
/* expression printer */
void prt_tok(struct tok *t) {
    switch(t->token)    {
    case TK_OPEN:  printf("( ");    break;
    case TK_CLOSE: printf(") ");    break;

    case TK_ADD:     printf("+ ");  break;
    case TK_SUB:     printf("- ");  break;
    case TK_MUL:     printf("* ");  break;
    case TK_MULI:  printf("*i "); break;
    case TK_POW:     printf("** "); break;
    case TK_DIV:     printf("/ ");  break;
    case TK_MOD:     printf("%% "); break;

    case TK_EQ:      printf("== "); break;
    case TK_NE:      printf("!= "); break;
    case TK_LT:      printf("< ");  break;
    case TK_GT:      printf("> ");  break;
    case TK_LE:      printf("<= "); break;
    case TK_GE:      printf(">= "); break;

    case TK_AND:     printf("&& "); break;
    case TK_BAND:  printf("& ");    break;
    case TK_BNOT:  printf("~ ");    break;
    case TK_BOR:     printf("| ");  break;
    case TK_BXOR:  printf("^ ");    break;
    case TK_NEG:     printf("_ ");  break;
    case TK_NOT:     printf("! ");  break;
    case TK_OR:      printf("|| "); break;
    case TK_SHL:     printf("<< "); break;
    case TK_SHR:     printf(">> "); break;

    case TK_ASSN:  printf("%s = ", t->name); break;
    case TK_FUNC:  printf("%s() ", t->funcid ? functable[(int)t->funcid-1] : t->name); break;
    case TK_VAL:     if (t->val.type == T_INT)
                                     printf("%ld ", t->val.ival);
                                 else
                                     printf("%g ", t->val.rval);
                                 break;

    case TK_VAR:     printf("%s ", t->name); break;
    default:             printf("?? (%d)", t->token); break;
    }
}

void prt_lst(struct tok *t) {
    for (; t; t=t->next) prt_tok(t);
    printf("\n");
}

#else
void prt_tok(struct tok *t) {}
void prt_lst(struct tok *t) {}
#endif



/*** UTILITY FUNCTIONS ***/

/* case-insensitive string comparison, TRUE or FALSE result */
int same_str(const char *a, const char *b) {
    if (!a || !b) return 0; /* false even if a == b == null */
    if (a == b) return 1;

#ifdef HAVE_STRCASECMP
    return (strcasecmp(a, b) == 0);
#elif HAVE_STRCMPI
    return (strcmpi(a, b) == 0);
#else
    while ((tolower((int)*a) == tolower((int)*b))) {
        if (!*a) return 1; /* if end of both strings, return true */
        a++; b++;
    }
    return 0; /* mismatch before end of string - return false */
#endif
}

/* case-insensitive string comparison with maximum length */
int same_str_len(const char *a, const char *b, int len) {
    if (!a || !b) return 0; /* false even if a == b == null */
    if (len == 0) return 0;
    if (a == b) return 1;

#ifdef HAVE_STRNCASECMP
    return (strncasecmp(a, b, len) == 0);
#elif HAVE_STRNCMPI
    return (strncmpi(a, b) == 0);
#else
    while (--len && (tolower((int)*a) == tolower((int)*b))) {
        if (!*a) return 1; /* true if both strings equal & end before len */
        a++; b++;
    }
    /* result based on last char of allowed length */
    return (tolower((int)*a) == tolower((int)*b)) ? 1 : 0;
#endif
}

#if EVAL_MALLOC
/* tracked memory allocation - create header */
struct memh *create_mem() {
    struct memh *mh = (struct memh *) malloc(sizeof(struct memh));
    mh->next = NULL;
    mh->ptr  = NULL;
    return mh;
}

/* tracked memory allocation - allocate memory using header */
void *mem_alloc(struct memh *mh, size_t len) {
    struct memh *mem = (struct memh *) malloc(len + sizeof(struct memh));
    if (!mem) return NULL;
    mem->next = mh->next;
    mh->next = mem;
    return mem->ptr = (void *) &mem[1];
}

/* tracked memory allocation - free all memory in header */
void free_mem(struct memh *mh) {
    struct memh *next;
    for (; mh; mh = next) {
        next = mh->next;
        free(mh);
    }
}

#else
#define MEM_BUFFER_LEN  4096
#define MAX_NUM_BUFFERS 2
char mem_buffer[MAX_NUM_BUFFERS][MEM_BUFFER_LEN];
struct memh mem_headers[MAX_NUM_BUFFERS];
int mem_idx[MAX_NUM_BUFFERS] = {0};

#define MEM_HEADER_TO_N(H) ((H)-mem_headers)

struct memh *create_mem() {
    int n;
    struct memh *mh;
    for(n=0;n<MAX_NUM_BUFFERS;n++) {
            if(mem_idx[n] == 0) {
                    mem_idx[n] = 1;
                    break;
            }
    }

    if(n == MAX_NUM_BUFFERS) {
#if MEM_DEBUG
            printf("create_mem: out of buffers\n", n);
#endif
            return NULL;
    }
#if MEM_DEBUG
    printf("create_mem: n is %d\n", n);
#endif
    mh = &mem_headers[n];
    memset(mh, 0, sizeof(struct memh));
    return mh;
}

void *mem_alloc(struct memh *mh, size_t len) {
    int n = MEM_HEADER_TO_N(mh);
    int mem_idx_n;
    struct memh *mem = NULL;
    len += sizeof(struct memh);
    mem_idx_n = mem_idx[n]-1;
#if MEM_DEBUG
    printf("mem_alloc: len is %d, buffer num is %d, buffer idx is %d, buffer len is %d\n", len, n, mem_idx_n, MEM_BUFFER_LEN);
#endif
    if(mem_idx_n + len > MEM_BUFFER_LEN) {
#if MEM_DEBUG
            printf("mem_alloc: out of mem\n");
#endif
            return NULL;
    }
    mem = (struct memh *) &mem_buffer[n][mem_idx_n];
    mem_idx[n] += len;
    mem->next = mh->next;
    mh->next = mem;
    return mem->ptr = (void *) &mem[1];
}

void free_mem(struct memh *mh) {
    int n = MEM_HEADER_TO_N(mh);
    mem_idx[n] = 0;
#if MEM_DEBUG
    printf("free_mem: n is %d\n", n);
#endif
}

#endif

/* creates an empty variable table */
struct vartable *create_vartable() {
    struct memh *mh = create_mem();
    struct vartable *vt;
    if(!mh) {
            return NULL;
    }
    vt = (struct vartable *) mem_alloc(mh, sizeof(struct vartable));
    if (mh && vt) vt->mh = mh, vt->first = NULL; else free_mem(mh);
    return vt;
}

/* frees a variable table */
void free_vartable(struct vartable *vt) {
    free_mem(vt->mh);
}

/* gets a variable out of a variable table */
struct var *get_var(struct vartable *vt, char *name) {
    struct var *v;
    if (!vt || !name) return NULL;
    for (v = vt->first; v; v = v->next) if (same_str(v->name, name)) return v;
    return NULL;
}

/* creates a new variable in a variable table */
struct var *put_var(struct vartable *vt, char *name, struct val *value) {
    struct var *v;
    char *n;

    if (!vt || !name || !value) return NULL;

    if ((v = get_var(vt, name))) {
        v->val = *value;
        return v;
    }

    v = (struct var *) mem_alloc(vt->mh, sizeof(struct var));
    n = (char *) mem_alloc(vt->mh, strlen(name)+1);
    if (v && n) {
        strcpy(n, name);
        v->name = n;
        v->val  = *value;
        v->next = vt->first;
        vt->first = v;
        return v;
    }
    return NULL;
}

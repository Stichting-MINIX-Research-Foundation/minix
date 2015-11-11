#include <magic_eval.h>
#include <magic.h>
#include <magic_eval_lib.h>

#define DEBUG                           MAGIC_DEBUG_SET(0)

#if DEBUG
#define MAGIC_EVAL_PRINTF _magic_printf
#else
#define MAGIC_EVAL_PRINTF magic_null_printf
#endif

PRIVATE int magic_eval_print_style = MAGIC_EVAL_PRINT_STYLE_DEFAULT;

/*===========================================================================*
 *                         magic_eval_get_var_cb                             *
 *===========================================================================*/
PRIVATE struct val* magic_eval_get_var_cb(char* name, struct val* val)
{
    _magic_selement_t selement;
    int ret;
    double dvalue;
    void *pvalue;
    unsigned long uvalue;
    long ivalue;
    char vvalue;
    struct _magic_dsentry dsentry_buff;
    MAGIC_EVAL_PRINTF("magic_eval_get_var_cb: %s requested\n", name);
    ret = magic_selement_lookup_by_name(name, &selement, &dsentry_buff);
    if(ret < 0) {
        return NULL;
    }
    val->type = T_INT;
    switch(selement.type->type_id) {
        case MAGIC_TYPE_FLOAT:
            dvalue = magic_selement_to_float(&selement);
            val->type = T_REAL;
            val->rval = dvalue;
        break;

        case MAGIC_TYPE_POINTER:
            pvalue = magic_selement_to_ptr(&selement);
            val->ival = (long) pvalue;
        break;

        case MAGIC_TYPE_INTEGER:
        case MAGIC_TYPE_ENUM:
            if(MAGIC_TYPE_FLAG(selement.type, MAGIC_TYPE_UNSIGNED)) {
                uvalue = magic_selement_to_unsigned(&selement);
                val->ival = (long) uvalue;
            }
            else {
                ivalue = magic_selement_to_int(&selement);
                val->ival = ivalue;
            }
        break;

        case MAGIC_TYPE_VOID:
            vvalue = *((char*) selement.address);
            val->ival = (long) vvalue;
        break;

        default:
            return NULL;
        break;
    }

    if(magic_eval_print_style & MAGIC_EVAL_PRINT_VAR_VALUES) {
        if(val->type == T_INT) {
            _magic_printf("VAR: %s = %ld\n", name, val->ival);
        }
        else {
            _magic_printf("VAR: %s = %g\n", name, val->rval);
        }
    }

    return val;
}

/*===========================================================================*
 *                     magic_eval_get_func_result_cb                         *
 *===========================================================================*/
PRIVATE struct val* magic_eval_get_func_result_cb(char* name, struct val* arg,
    struct val* ret)
{
    struct _magic_function *function;
    magic_eval_func_t magic_eval_func;
    long result, iarg;
    MAGIC_EVAL_PRINTF("magic_eval_get_func_result_cb: %s requested\n", name);
    if(strncmp(MAGIC_EVAL_FUNC_PREFIX, name, strlen(MAGIC_EVAL_FUNC_PREFIX))) {
        return NULL;
    }

    function = magic_function_lookup_by_name(NULL, name);
    if(!function) {
        return NULL;
    }
    magic_eval_func = (magic_eval_func_t) (long) function->address;
    iarg = arg->type == T_INT ? arg->ival : (long) arg->rval;
    result = magic_eval_func(iarg);
    ret->type = T_INT;
    ret->ival = result;

    if(magic_eval_print_style & MAGIC_EVAL_PRINT_FUNC_RESULTS) {
        _magic_printf("FUNCTION: %s(%ld) = %ld\n", name, iarg, result);
    }

    return ret;
}

/*===========================================================================*
 *                             magic_eval_init                               *
 *===========================================================================*/
PUBLIC void magic_eval_init()
{
    eval_set_cb_get_var(magic_eval_get_var_cb);
    eval_set_cb_get_func_result(magic_eval_get_func_result_cb);
}

/*===========================================================================*
 *                               magic_eval                                  *
 *===========================================================================*/
PRIVATE int magic_eval(char *expr, struct val* result)
{
    int ret;
    MAGIC_EVAL_PRINTF("magic_eval: Evaluating expression %s\n", expr);
    ret = evaluate(expr, result, NULL);
    switch(ret) {
        case ERROR_SYNTAX:
            ret = MAGIC_EINVAL;
        break;
        case ERROR_FUNCNOTFOUND:
        case ERROR_VARNOTFOUND:
            ret = MAGIC_ENOENT;
        break;
        case ERROR_NOMEM:
            ret = MAGIC_ENOMEM;
        break;
        case ERROR_DIV0:
            ret = MAGIC_EBADMSTATE;
        break;
        case RESULT_OK:
            ret = 0;
        break;
        default:
            ret = MAGIC_EGENERIC;
        break;
    }

    return ret;
}

/*===========================================================================*
 *                             magic_eval_int                                *
 *===========================================================================*/
PUBLIC int magic_eval_int(char *expr, long *result)
{
    struct val val_res;
    int ret;
    ret = magic_eval(expr, &val_res);
    if(ret < 0) {
        return ret;
    }
    *result = val_res.type == T_INT ? val_res.ival : (long) val_res.rval;
    return 0;
}

/*===========================================================================*
 *                             magic_eval_bool                               *
 *===========================================================================*/
PUBLIC int magic_eval_bool(char *expr, char *result)
{
    struct val val_res;
    int ret;
    ret = magic_eval(expr, &val_res);
    if(ret < 0) {
        return ret;
    }
    if(val_res.type != T_INT) {
        return MAGIC_EINVAL;
    }
    *result = val_res.ival == 0 ? 0 : 1;
    return 0;
}

/*===========================================================================*
 *                            magic_eval_float                               *
 *===========================================================================*/
PUBLIC int magic_eval_float(char *expr, double *result)
{
    struct val val_res;
    int ret;
    ret = magic_eval(expr, &val_res);
    if(ret < 0) {
        return ret;
    }
    *result = val_res.type == T_INT ? (double) val_res.ival : val_res.rval;
    return 0;
}

/*===========================================================================*
 *                       magic_eval_get_print_style                          *
 *===========================================================================*/
PUBLIC int magic_eval_get_print_style()
{
    return magic_eval_print_style;
}

/*===========================================================================*
 *                       magic_eval_set_print_style                          *
 *===========================================================================*/
PUBLIC void magic_eval_set_print_style(int style)
{
    magic_eval_print_style = style;
}


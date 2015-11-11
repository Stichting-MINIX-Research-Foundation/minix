#undef xglue
#undef glue
#undef CALLBACK_TYPENAME
#undef CALLBACK_SETTERNAME
#undef DEFINE_CALLBACK
#undef DECLARE_CALLBACK

#define xglue(x, y) x ## y
#define glue(x, y) xglue(x, y)

#ifdef CALLBACK_FAMILY
#define CALLBACK_TYPENAME(name) glue(glue(glue(glue(glue(CALLBACK_PREFIX, _cb_), CALLBACK_FAMILY), _), name), _t)
#define CALLBACK_SETTERNAME(name) glue(glue(glue(glue(CALLBACK_PREFIX, _setcb_), CALLBACK_FAMILY), _), name)
#else
#define CALLBACK_TYPENAME(name) glue(glue(glue(CALLBACK_PREFIX, _cb_), name), _t)
#define CALLBACK_SETTERNAME(name) glue(glue(CALLBACK_PREFIX, _setcb_), name)
#endif

#define DECLARE_CALLBACK(ret_type, name, args)                              \
typedef ret_type(*CALLBACK_TYPENAME(name))args

#define DEFINE_DECL_CALLBACK(ret_type, name, args)                          \
DECLARE_CALLBACK(ret_type, name, args);                                     \
void CALLBACK_SETTERNAME(name)(CALLBACK_TYPENAME(name) cb)

#define DEFINE_DECL_CALLBACK_CUSTOM(ret_type, name, args, setter_args)      \
DECLARE_CALLBACK(ret_type, name, args);                                     \
void CALLBACK_SETTERNAME(name)setter_args

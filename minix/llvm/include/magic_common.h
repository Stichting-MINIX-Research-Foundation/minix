#ifndef _MAGIC_COMMON_H
#define _MAGIC_COMMON_H

/* Magic constants. */
#define MAGIC_PREFIX                        magic_
#define MAGIC_PREFIX_STR                    "magic_"
#define MAGIC_ASR_PREFIX                    magic_asr_
#define MAGIC_ASR_PREFIX_STR                "magic_asr_"
#define MAGIC_NESTED_PREFIX_STR             "nested_"
#define MAGIC_EVAL_FUNC_PREFIX              "me_"
#define MAGIC_ANON_MEMBER_PREFIX            "magic.anon"
#define MAGIC_STRINGREF_HAS_MAGIC_HIDDEN_PREFIX(S)                             \
    ((S).startswith(MAGIC_HIDDEN_ARRAY_PREFIX)                                 \
    || (S).startswith(MAGIC_HIDDEN_STR_PREFIX))

#define MAGIC_VOID_PTR                      _____magic_instr_void_ptr
#define MAGIC_VOID_PTR_NAME                 "_____magic_instr_void_ptr"
#define MAGIC_VOID_ARRAY                    _____magic_instr_void_arr
#define MAGIC_VOID_ARRAY_NAME               "_____magic_instr_void_arr"

#define MAGIC_VOID_PTR_TYPE                 _magic_void_ptr_type_ptr
#define MAGIC_VOID_PTR_TYPE_ID              1
#define MAGIC_VOID_PTR_INT_CAST_TYPE        _magic_void_ptr_int_cast_type_ptr
#define MAGIC_VOID_PTR_INT_CAST_TYPE_ID     2
#define MAGIC_VOID_ARRAY_TYPE               _magic_void_array_type_ptr
#define MAGIC_VOID_ARRAY_TYPE_ID            3
#define MAGIC_PTRINT_TYPE                   _magic_ptrint_type_ptr
#define MAGIC_PTRINT_TYPE_ID                4
#define MAGIC_PTRINT_ARRAY_TYPE             _magic_ptrint_array_type_ptr
#define MAGIC_PTRINT_ARRAY_TYPE_ID          5
#define MAGIC_VOID_TYPE                     (MAGIC_VOID_PTR_TYPE->contained_types[0])

#ifdef __MINIX
#define GLOBAL_VARS_IN_SECTION              1
#else
#define GLOBAL_VARS_IN_SECTION              0
#endif
#define GLOBAL_VARS_SECTION_PREFIX          ".gvars"
#define GLOBAL_VARS_SECTION_DATA            GLOBAL_VARS_SECTION_PREFIX
#define GLOBAL_VARS_SECTION_RO              (GLOBAL_VARS_SECTION_PREFIX "_ro")

#define MAGIC_LLVM_METADATA_SECTION         "llvm.metadata"
#define MAGIC_DEFAULT_EXT_LIB_SECTION_REGEX "^.lib.*"
#define MAGIC_STATIC_FUNCTIONS_SECTION      "magic_functions"

#define MAGIC_STATIC_VARS_SECTION_PREFIX    "magic_data"
#define MAGIC_STATIC_VARS_SECTION_DATA      MAGIC_STATIC_VARS_SECTION_PREFIX
#define MAGIC_STATIC_VARS_SECTION_RO        (MAGIC_STATIC_VARS_SECTION_PREFIX "_ro")

#define MAGIC_SHADOW_VARS_SECTION_PREFIX    "magic_shadow_data"
#define MAGIC_SHADOW_VARS_SECTION_DATA      MAGIC_SHADOW_VARS_SECTION_PREFIX
#define MAGIC_SHADOW_VARS_SECTION_RO        (MAGIC_SHADOW_VARS_SECTION_PREFIX "_ro")

#define UNBL_SECTION_PREFIX                 "unblockify"
#define MAGIC_SHADOW_VAR_PREFIX             ".magic_shadow_"
#define MAGIC_HIDDEN_ARRAY_PREFIX           ".arr.magic"
#define MAGIC_HIDDEN_STR_PREFIX             ".str.magic"

#define MAGIC_MALLOC_VARS_SECTION_PREFIX    "magic_malloc_data"

/* Magic configuration. */
#ifndef MAGIC_OUTPUT_CTL
#define MAGIC_OUTPUT_CTL                    0
#endif
/* 0=disabled, 1=force no debug output, 2=force no output (for perf. testing).*/
#define MAGIC_CHECK_LEVEL                   1
/* 2=extra checks, 1=standard checks, 0=no checks (for perf. testing). */
#define MAGIC_FLATTEN_FUNCTION_ARGS         0 /* XXX was 1 but header was not included, seems to break on variadic functions */
#define MAGIC_CHECK_INVARIANTS              1
#define MAGIC_SHRINK_TYPE_STR               1
#define MAGIC_MAX_NAME_LEN                  64
#define MAGIC_MAX_TYPE_STR_LEN              256
#define MAGIC_MAX_RECURSIVE_TYPES           1024
#define MAGIC_TYPE_STR_PRINT_MAX            5000
#define MAGIC_TYPE_STR_PRINT_MAX_LEVEL      10
#define MAGIC_MAX_DEAD_DSENTRIES            10
#define MAGIC_MAX_DEAD_DSENTRIES_SIZE       (1024 * 4 * 10)
#define MAGIC_NAMED_ALLOC_USE_DBG_INFO      0
/* 1=for more verbose dsentry naming. */
#define MAGIC_FORCE_ALLOC_EXT_NAMES         0
/* 1=to force external names for allocations made inside library functions. */
#define MAGIC_ABORT_ON_UNSUPPORTED_LOCAL_EXTERNAL_TYPE 0
/* 0=to resort to void* type when a local external type is not supported. */
#ifndef MAGIC_MEM_USAGE_OUTPUT_CTL
#define MAGIC_MEM_USAGE_OUTPUT_CTL          0
#endif
/* 0=disabled, 1=use call site info 2=use stacktrace */

#define MAGIC_INSTRUMENT_MEM_FUNCS_ASR_ONLY 0
#define MAGIC_INSTRUMENT_MEM_CUSTOM_WRAPPERS 1
#define MAGIC_INSTRUMENT_MEM_FUNCS          1
#define MAGIC_INSTRUMENT_STACK              1
#define MAGIC_FORCE_RAW_UNIONS              0
#define MAGIC_FORCE_RAW_BITFIELDS           0
#define MAGIC_FORCE_DYN_MEM_ZERO_INIT       0
/* 1=for accurate dsentry analysis. */
#define MAGIC_INDEX_DYN_LIBS                1
#define MAGIC_USE_DYN_MEM_WRAPPERS          1
#define MAGIC_USE_DYN_DL_WRAPPERS           1
#define MAGIC_ALLOW_DYN_MEM_WRAPPER_NESTING 1

/* qprof-related settings */
#ifdef __MINIX
#define MAGIC_USE_QPROF_INSTRUMENTATION     0
#else
#define MAGIC_USE_QPROF_INSTRUMENTATION     1
#endif
#define MAGIC_DEEPEST_LL_LOOP_HOOK          magic_deepest_ll_loop
#define MAGIC_DEEPEST_LL_LIB_HOOK           magic_deepest_ll_lib
#define MAGIC_DEEPEST_LL_LOOP_HOOK_NAME     "magic_deepest_ll_loop"
#define MAGIC_DEEPEST_LL_LIB_HOOK_NAME      "magic_deepest_ll_lib"
#define MAGIC_NUM_LL_TASK_CLASSES           magic_num_ll_task_classes
#define MAGIC_NUM_LL_BLOCK_EXT_TASK_CLASSES magic_num_ll_block_ext_task_classes
#define MAGIC_NUM_LL_BLOCK_INT_TASK_CLASSES magic_num_ll_block_int_task_classes
#define MAGIC_NUM_LL_BLOCK_EXT_LIBS         magic_num_ll_block_ext_libs
#define MAGIC_NUM_LL_BLOCK_INT_LIBS         magic_num_ll_block_int_libs
#define MAGIC_NUM_LL_TASK_CLASSES_NAME           "magic_num_ll_task_classes"
#define MAGIC_NUM_LL_BLOCK_EXT_TASK_CLASSES_NAME "magic_num_ll_block_ext_task_classes"
#define MAGIC_NUM_LL_BLOCK_INT_TASK_CLASSES_NAME "magic_num_ll_block_int_task_classes"
#define MAGIC_NUM_LL_BLOCK_EXT_LIBS_NAME    "magic_num_ll_block_ext_libs"
#define MAGIC_NUM_LL_BLOCK_INT_LIBS_NAME    "magic_num_ll_block_int_libs"

#define MAGIC_THREAD_SAFE                   1
#define MAGIC_FORCE_LOCKS                   0
#define MAGIC_LOOKUP_SENTRY                 1
#define MAGIC_LOOKUP_DSENTRY                1
#define MAGIC_LOOKUP_FUNCTION               1
#define MAGIC_LOOKUP_DFUNCTION              1
#define MAGIC_LOOKUP_TYPE                   1
#define MAGIC_LOOKUP_SENTRY_ALLOW_RANGE_INDEX 1
#define MAGIC_LOOKUP_SENTRY_ALLOW_NAME_HASH 1
#define MAGIC_LOOKUP_FUNCTION_ALLOW_ADDR_HASH 1

#define MAGIC_INDEX_INT_CAST                1
#define MAGIC_INDEX_FUN_PTR_INT_CAST        1
#define MAGIC_INDEX_STR_PTR_INT_CAST        1
#define MAGIC_INDEX_VOID_PTR_INT_CAST       1
#define MAGIC_INDEX_OTH_PTR_INT_CAST        1

#define MAGIC_INDEX_BIT_CAST                1
#define MAGIC_INDEX_TRANSITIVE_BIT_CASTS    0
#define MAGIC_INDEX_FUN_PTR_BIT_CAST        1
#define MAGIC_INDEX_STR_PTR_BIT_CAST        1
#define MAGIC_INDEX_VOID_PTR_BIT_CAST       0
#define MAGIC_INDEX_OTH_PTR_BIT_CAST        1

#ifdef __MINIX
#define MAGIC_SKIP_TOVOID_PTR_BIT_CAST      0
#else
#define MAGIC_SKIP_TOVOID_PTR_BIT_CAST      1
#endif

#define MAGIC_COMPACT_COMP_TYPES            0

#define MAGIC_OFF_BY_N_PROTECTION_N         0

#define MAGIC_VARSIZED_STRUCTS_SUPPORT      1

#define MAGIC_ALLOW_DEAD_DSENTRIES_DEFAULT  0

#define MAGIC_WALK_UNIONS_AS_VOID_ARRAYS_DEFAULT 1

#define MAGIC_DEBUG_SELECT(D,ND)            (MAGIC_OUTPUT_CTL>=1 ? (ND) : (D))
#define MAGIC_OUTPUT_SELECT(D,ND,NO)                                           \
    (MAGIC_OUTPUT_CTL>=2 ? (NO) : MAGIC_DEBUG_SELECT(D,ND))
#define MAGIC_DEBUG_SET(D)                  MAGIC_DEBUG_SELECT(D,0)

#define MAGIC_ENABLED                       "_magic_enabled"

#define MAGIC_ARRAY_NAME                    "_magic_sentries_array"
#define MAGIC_TYPE_ARRAY_NAME               "_magic_types_array"
#define MAGIC_FUNC_ARRAY_NAME               "_magic_functions_array"
#define MAGIC_DSINDEX_ARRAY_NAME            "_magic_dsindexes_array"

#define MAGIC_ROOT_VAR_NAME                 "_magic_vars_buff"
#define MAGIC_RSTRUCT_FIELD_ASR_SEED                  "asr_seed"
#define MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAP_DO_PERMUTATE "asr_heap_map_do_permutate"
#define MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAX_OFFSET       "asr_heap_max_offset"
#define MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAX_PADDING      "asr_heap_max_padding"
#define MAGIC_RSTRUCT_FIELD_ASR_MAP_MAX_OFFSET_PAGES  "asr_map_max_offset_pages"
#define MAGIC_RSTRUCT_FIELD_ASR_MAP_MAX_PADDING_PAGES "asr_map_max_padding_pages"
#define MAGIC_RSTRUCT_FIELD_NO_MEM_INST         "no_mem_inst"
#define MAGIC_RSTRUCT_FIELD_TYPES               "types"
#define MAGIC_RSTRUCT_FIELD_TYPES_NUM           "types_num"
#define MAGIC_RSTRUCT_FIELD_TYPES_NEXT_ID       "types_next_id"
#define MAGIC_RSTRUCT_FIELD_FUNCTIONS           "functions"
#define MAGIC_RSTRUCT_FIELD_FUNCTIONS_NUM       "functions_num"
#define MAGIC_RSTRUCT_FIELD_FUNCTIONS_NEXT_ID   "functions_next_id"
#define MAGIC_RSTRUCT_FIELD_SENTRIES            "sentries"
#define MAGIC_RSTRUCT_FIELD_SENTRIES_NUM        "sentries_num"
#define MAGIC_RSTRUCT_FIELD_SENTRIES_STR_NUM    "sentries_str_num"
#define MAGIC_RSTRUCT_FIELD_SENTRIES_NEXT_ID    "sentries_next_id"
#define MAGIC_RSTRUCT_FIELD_DSINDEXES           "dsindexes"
#define MAGIC_RSTRUCT_FIELD_DSINDEXES_NUM       "dsindexes_num"
#define MAGIC_RSTRUCT_FIELD_FIRST_DSENTRY   "first_dsentry"
#define MAGIC_RSTRUCT_FIELDS                                                   \
        MAGIC_RSTRUCT_FIELD_ASR_SEED,                                          \
        MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAP_DO_PERMUTATE,                         \
        MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAX_OFFSET,                               \
        MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAX_PADDING,                              \
        MAGIC_RSTRUCT_FIELD_ASR_MAP_MAX_OFFSET_PAGES,                          \
        MAGIC_RSTRUCT_FIELD_ASR_MAP_MAX_PADDING_PAGES,                         \
        MAGIC_RSTRUCT_FIELD_NO_MEM_INST,                                       \
        MAGIC_RSTRUCT_FIELD_TYPES,                                             \
        MAGIC_RSTRUCT_FIELD_TYPES_NUM,                                         \
        MAGIC_RSTRUCT_FIELD_TYPES_NEXT_ID,                                     \
        MAGIC_RSTRUCT_FIELD_FUNCTIONS,                                         \
        MAGIC_RSTRUCT_FIELD_FUNCTIONS_NUM,                                     \
        MAGIC_RSTRUCT_FIELD_FUNCTIONS_NEXT_ID,                                 \
        MAGIC_RSTRUCT_FIELD_SENTRIES,                                          \
        MAGIC_RSTRUCT_FIELD_SENTRIES_NUM,                                      \
        MAGIC_RSTRUCT_FIELD_SENTRIES_STR_NUM,                                  \
        MAGIC_RSTRUCT_FIELD_SENTRIES_NEXT_ID,                                  \
        MAGIC_RSTRUCT_FIELD_DSINDEXES,                                         \
        MAGIC_RSTRUCT_FIELD_DSINDEXES_NUM,                                     \
        MAGIC_RSTRUCT_FIELD_FIRST_DSENTRY

#define MAGIC_ENTRY_POINT                       "main"
#define MAGIC_INIT_FUNC_NAME                    "magic_init"
#define MAGIC_DATA_INIT_FUNC_NAME               "magic_data_init"

#define MAGIC_STACK_DSENTRIES_CREATE_FUNC_NAME  "magic_stack_dsentries_create"
#define MAGIC_STACK_DSENTRIES_DESTROY_FUNC_NAME "magic_stack_dsentries_destroy"

#define MAGIC_GET_PAGE_SIZE_FUNC_NAME           "magic_get_sys_pagesize"

#define MAGIC_VOID_PTR_TYPE_PTR_NAME            "_magic_void_ptr_type_ptr"

#define MAGIC_ALLOC_NAME_SUFFIX                 "#"
#define MAGIC_ALLOC_NAME_SEP                    "%"
#define MAGIC_ALLOC_NONAME                      "%UNKNOWN"
#define MAGIC_ALLOC_EXT_NAME                    "%EXT"
#define MAGIC_ALLOC_EXT_PARENT_NAME             "%EXT_PARENT"
#define MAGIC_ALLOC_RET_ADDR_NAME               "%RET_ADDR"
#define MAGIC_ALLOC_INITIAL_STACK_NAME          "%INITIAL_STACK_AREA"
#define MAGIC_OBDSENTRY_DEFAULT_PARENT_NAME     "%OUT_OF_BAND_PARENT"
#define MAGIC_DSENTRY_DATA_SEGMENT_NAME         "%LIB_DATA_SEGMENT"
#define MAGIC_DSENTRY_ABS_NAME_SEP              "~"
#define MAGIC_SELEMENT_SEP                      "/"
#define MAGIC_NAME_INVALID                      "%INVALID"

#define MAGIC_SSTRUCT_FIELD_ID                  "id"
#define MAGIC_SSTRUCT_FIELD_NAME                "name"
#define MAGIC_SSTRUCT_FIELD_TYPE                "type"
#define MAGIC_SSTRUCT_FIELD_FLAGS               "flags"
#define MAGIC_SSTRUCT_FIELD_ADDRESS             "address"
#define MAGIC_SSTRUCT_FIELD_SHADOW_ADDRESS      "shadow_address"
#define MAGIC_SSTRUCT_FIELDS                                                   \
        MAGIC_SSTRUCT_FIELD_ID,                                                \
        MAGIC_SSTRUCT_FIELD_NAME,                                              \
        MAGIC_SSTRUCT_FIELD_TYPE,                                              \
        MAGIC_SSTRUCT_FIELD_FLAGS,                                             \
        MAGIC_SSTRUCT_FIELD_ADDRESS,                                           \
        MAGIC_SSTRUCT_FIELD_SHADOW_ADDRESS

#define MAGIC_TSTRUCT_FIELD_ID                  "id"
#define MAGIC_TSTRUCT_FIELD_NAME                "name"
#define MAGIC_TSTRUCT_FIELD_NAMES               "names"
#define MAGIC_TSTRUCT_FIELD_NUM_NAMES           "num_names"
#define MAGIC_TSTRUCT_FIELD_TYPE_STR            "type_str"
#define MAGIC_TSTRUCT_FIELD_SIZE                "size"
#define MAGIC_TSTRUCT_FIELD_NUM_CHILD_TYPES     "num_child_types"
#define MAGIC_TSTRUCT_FIELD_CONTAINED_TYPES     "contained_types"
#define MAGIC_TSTRUCT_FIELD_COMPATIBLE_TYPES    "compatible_types"
#define MAGIC_TSTRUCT_FIELD_MEMBER_NAMES        "member_names"
#define MAGIC_TSTRUCT_FIELD_MEMBER_OFFSETS      "member_offsets"
#define MAGIC_TSTRUCT_FIELD_VALUE_SET           "value_set"
#define MAGIC_TSTRUCT_FIELD_TYPE_ID             "type_id"
#define MAGIC_TSTRUCT_FIELD_FLAGS               "flags"
#define MAGIC_TSTRUCT_FIELD_BIT_WIDTH           "bit_width"
#define MAGIC_TSTRUCT_FIELDS                                                   \
        MAGIC_TSTRUCT_FIELD_ID,                                                \
        MAGIC_TSTRUCT_FIELD_NAME,                                              \
        MAGIC_TSTRUCT_FIELD_NAMES,                                             \
        MAGIC_TSTRUCT_FIELD_NUM_NAMES,                                         \
        MAGIC_TSTRUCT_FIELD_TYPE_STR,                                          \
        MAGIC_TSTRUCT_FIELD_SIZE,                                              \
        MAGIC_TSTRUCT_FIELD_NUM_CHILD_TYPES,                                   \
        MAGIC_TSTRUCT_FIELD_CONTAINED_TYPES,                                   \
        MAGIC_TSTRUCT_FIELD_COMPATIBLE_TYPES,                                  \
        MAGIC_TSTRUCT_FIELD_MEMBER_NAMES,                                      \
        MAGIC_TSTRUCT_FIELD_MEMBER_OFFSETS,                                    \
        MAGIC_TSTRUCT_FIELD_VALUE_SET,                                         \
        MAGIC_TSTRUCT_FIELD_TYPE_ID,                                           \
        MAGIC_TSTRUCT_FIELD_FLAGS,                                             \
        MAGIC_TSTRUCT_FIELD_BIT_WIDTH

#define MAGIC_FSTRUCT_FIELD_ID                   "id"
#define MAGIC_FSTRUCT_FIELD_NAME                 "name"
#define MAGIC_FSTRUCT_FIELD_TYPE                 "type"
#define MAGIC_FSTRUCT_FIELD_FLAGS                "flags"
#define MAGIC_FSTRUCT_FIELD_ADDRESS              "address"
#define MAGIC_FSTRUCT_FIELDS                                                   \
        MAGIC_FSTRUCT_FIELD_ID,                                                \
        MAGIC_FSTRUCT_FIELD_NAME,                                              \
        MAGIC_FSTRUCT_FIELD_TYPE,                                              \
        MAGIC_FSTRUCT_FIELD_FLAGS,                                             \
        MAGIC_FSTRUCT_FIELD_ADDRESS

#define MAGIC_DSTRUCT_FIELD_TYPE                 "type"
#define MAGIC_DSTRUCT_FIELD_NAME                 "name"
#define MAGIC_DSTRUCT_FIELD_PARENT_NAME          "parent_name"
#define MAGIC_DSTRUCT_FIELD_FLAGS                "flags"
#define MAGIC_DSTRUCT_FIELDS                                                   \
        MAGIC_DSTRUCT_FIELD_TYPE,                                              \
        MAGIC_DSTRUCT_FIELD_NAME,                                              \
        MAGIC_DSTRUCT_FIELD_PARENT_NAME,                                       \
        MAGIC_DSTRUCT_FIELD_FLAGS

#define MAGIC_TYPE_ISUNION                  0x01
#define MAGIC_TYPE_ISPADDED                 0x02

/* Type IDs. */
#define MAGIC_TYPE_VOID                     1
#define MAGIC_TYPE_FLOAT                    2
#define MAGIC_TYPE_INTEGER                  3
#define MAGIC_TYPE_FUNCTION                 4
#define MAGIC_TYPE_ARRAY                    5
#define MAGIC_TYPE_ENUM                     6
#define MAGIC_TYPE_VECTOR                   7
#define MAGIC_TYPE_UNION                    8
#define MAGIC_TYPE_STRUCT                   9
#define MAGIC_TYPE_POINTER                  10
#define MAGIC_TYPE_OPAQUE                   11

/* Type flags. */
#define MAGIC_TYPE_EXTERNAL                 0x001
#define MAGIC_TYPE_IS_ROOT                  0x002
#define MAGIC_TYPE_DYNAMIC                  0x004
#define MAGIC_TYPE_INT_CAST                 0x008
#define MAGIC_TYPE_STRICT_VALUE_SET         0x010
#define MAGIC_TYPE_VARSIZE                  0x020
#define MAGIC_TYPE_UNSIGNED                 0x040
#define MAGIC_TYPE_NO_INNER_PTRS            0x080

/* State flags for sentries and functions. */
#define MAGIC_STATE_DIRTY                   0x00000001
#define MAGIC_STATE_CONSTANT                0x00000002
#define MAGIC_STATE_DYNAMIC                 0x00000004
#define MAGIC_STATE_DETACHED                0x00000008
#define MAGIC_STATE_DATA                    0x00000010
#define MAGIC_STATE_HEAP                    0x00000020
#define MAGIC_STATE_MAP                     0x00000040
#define MAGIC_STATE_SHM                     0x00000080
#define MAGIC_STATE_STACK                   0x00000100
#define MAGIC_STATE_TEXT                    0x00000200
/* All libraries. */
#define MAGIC_STATE_LIB                     0x00000400
/* Dynamically linked libraries. */
#define MAGIC_STATE_LIB_SO                  0x00000800
/* Dynamically loaded libraries. */
#define MAGIC_STATE_LIB_DSO                 0x00001000
#define MAGIC_STATE_ADDR_NOT_TAKEN          0x00002000
#define MAGIC_STATE_EXT                     0x00004000
#define MAGIC_STATE_OUT_OF_BAND             0x00008000
#define MAGIC_STATE_STRING                  0x00010000
#define MAGIC_STATE_NAMED_STRING            0x00020000
#define MAGIC_STATE_MODE_R                  0x00040000
#define MAGIC_STATE_MODE_W                  0x00080000
#define MAGIC_STATE_MODE_X                  0x00100000
#define MAGIC_STATE_THREAD_LOCAL            0x00200000
#define MAGIC_STATE_MEMPOOL                 0x00400000
#define MAGIC_STATE_MEMBLOCK                0x00800000
#define MAGIC_STATE_EXTERNAL                0x01000000
#define MAGIC_STATE_TYPE_SIZE_MISMATCH      0x02000000
#define MAGIC_STATE_IMMUTABLE               0x04000000
#define MAGIC_STATE_INIT                    0x08000000
#define MAGIC_STATE_DIRTY_PAGE              0x10000000
/* Skip char* and void* entries in arrays */
#define MAGIC_STATE_SKIP_BYTE_INDICES       0x20000000

#define MAGIC_STATE_ANNOTATION_MASK                                            \
    (MAGIC_STATE_MODE_R | MAGIC_STATE_MODE_W | MAGIC_STATE_MODE_X)

#define MAGIC_ASR_FLAG_INIT                 0x40000

#define MAGIC_STATE_EXTF_MASK               0xFF000000
#define MAGIC_STATE_EXTF_SHIFT              24

#define MAGIC_STATE_FLAG(E,F) (((E)->flags & (F)) != 0)
#define MAGIC_STATE_FLAGS(E,F) (((E)->flags & (F)) == (F))
#define MAGIC_STATE_FLAGS_REGION(F)                                            \
    ((F) & (MAGIC_STATE_DATA | MAGIC_STATE_HEAP | MAGIC_STATE_MAP              \
    | MAGIC_STATE_SHM | MAGIC_STATE_STACK | MAGIC_STATE_TEXT))
#define MAGIC_STATE_FLAGS_LIBSPEC(F)                                           \
    ((F) & (MAGIC_STATE_LIB | MAGIC_STATE_LIB_SO | MAGIC_STATE_LIB_DSO))
#define MAGIC_STATE_REGION(E)  MAGIC_STATE_FLAGS_REGION((E)->flags)
#define MAGIC_STATE_LIBSPEC(E) MAGIC_STATE_FLAGS_LIBSPEC((E)->flags)
#define MAGIC_STATE_FLAGS_REGION_C(F)                                          \
    (((F) & MAGIC_STATE_DATA) ? 'D' : ((F) & MAGIC_STATE_HEAP) ? 'H'           \
    : ((F) & MAGIC_STATE_SHM) ? 'X' : ((F) & MAGIC_STATE_MAP) ? 'M'            \
    : ((F) & MAGIC_STATE_STACK) ? 'S' : ((F) & MAGIC_STATE_TEXT) ? 'T' : '?')
#define MAGIC_STATE_FLAGS_LIBSPEC_C(F)                                         \
    (((F) & MAGIC_STATE_LIB) ? (((F) & MAGIC_STATE_LIB_SO) ? 'l'               \
    : ((F) & MAGIC_STATE_LIB_DSO) ? 'o' : 'L') : '0')
#define MAGIC_STATE_REGION_C(E)  MAGIC_STATE_FLAGS_REGION_C((E)->flags)
#define MAGIC_STATE_LIBSPEC_C(E) MAGIC_STATE_FLAGS_LIBSPEC_C((E)->flags)

#define MAGIC_STATE_IS_EXTF(F)       (((F) & MAGIC_STATE_EXTF_MASK) == (F))
#define MAGIC_STATE_FLAGS_TO_EXTF(F)                                           \
    (((F) & MAGIC_STATE_EXTF_MASK) >> MAGIC_STATE_EXTF_SHIFT)
#define MAGIC_STATE_FLAGS_TO_NONEXTF(F) ((F) & (~MAGIC_STATE_EXTF_MASK))
#define MAGIC_STATE_EXTF_TO_FLAGS(F)                                           \
    (((F) << MAGIC_STATE_EXTF_SHIFT) & MAGIC_STATE_EXTF_MASK)
#define MAGIC_STATE_EXTF_FLAG(E,F)   (MAGIC_STATE_EXTF_GET(E,F) != 0)
#define MAGIC_STATE_EXTF_GET(E,F)                                              \
    ((MAGIC_STATE_FLAGS_TO_EXTF((E)->flags) & (F)))
#define MAGIC_STATE_EXTF_ADD(E,F)                                              \
    ((E)->flags |= MAGIC_STATE_EXTF_TO_FLAGS(F))
#define MAGIC_STATE_EXTF_DEL(E,F)                                              \
    ((E)->flags &= ~MAGIC_STATE_EXTF_TO_FLAGS(F))
#define MAGIC_STATE_EXTF_SET(E,F)                                              \
    do {                                                                       \
        MAGIC_STATE_EXTF_CLEAR(E);                                             \
        (E)->flags |= MAGIC_STATE_EXTF_TO_FLAGS(F);                            \
    } while(0)
#define MAGIC_STATE_EXTF_CLEAR(E)    ((E)->flags &= ~MAGIC_STATE_EXTF_MASK)

/* Annotations. */
#define MAGIC_CALL_ANNOTATION_VAR               _magic_call_annotation_var
#define MAGIC_CALL_ANNOTATION_VAR_NAME          "_magic_call_annotation_var"
#define MAGIC_CALL_ANNOTATE(C, VALUE)                                          \
    do { C; MAGIC_CALL_ANNOTATION_VAR = VALUE; } while(0)
#define MAGIC_CALL_MEM_SKIP_INSTRUMENTATION     0x01

#define MAGIC_VAR_ANNOTATION_PREFIX_NAME        "_magic_var_annotation_"

#define MAGIC_VAR_ANNOTATE(T,V,A)                                              \
T V;                                                                           \
volatile int _magic_var_annotation_ ## V = A
#define MAGIC_VAR_INIT_ANNOTATE(T,V,I,A)                                       \
T V = I;                                                                       \
volatile int _magic_var_annotation_ ## V = A

#define MAGIC_MEMCPY_FUNC_NAME        "memcpy"
#define MAGIC_MALLOC_FUNC_NAME        "malloc"

/* Magic memory pool management functions. */
#define MAGIC_MEMPOOL_BLOCK_ALLOC_TEMPLATE_FUNC_NAME                           \
    "mempool_block_alloc_template"

/* Wrapper functions. */
#define MAGIC_MEMPOOL_CREATE_FUNCS                                             \
    __X(magic_mempool_create_begin), __X(magic_mempool_create_end)
#define MAGIC_MEMPOOL_DESTROY_FUNCS                                            \
    __X(magic_mempool_destroy_begin), __X(magic_mempool_destroy_end)
#define MAGIC_MEMPOOL_MGMT_FUNCS                                               \
    __X(magic_mempool_mgmt_begin), __X(magic_mempool_mgmt_end)
#define MAGIC_MEMPOOL_RESET_FUNCS                                              \
    __X(magic_mempool_reset_begin), __X(magic_mempool_mgmt_end)
#define MAGIC_MEMPOOL_FUNCS                                                    \
    MAGIC_MEMPOOL_CREATE_FUNCS,                                                \
    MAGIC_MEMPOOL_DESTROY_FUNCS,                                               \
    MAGIC_MEMPOOL_MGMT_FUNCS,                                                  \
    MAGIC_MEMPOOL_RESET_FUNCS

#define MAGIC_MEMPOOL_CREATE_FUNC_NAMES     MAGIC_MEMPOOL_CREATE_FUNCS, ""
#define MAGIC_MEMPOOL_DESTROY_FUNC_NAMES    MAGIC_MEMPOOL_DESTROY_FUNCS, ""
#define MAGIC_MEMPOOL_MGMT_FUNC_NAMES       MAGIC_MEMPOOL_MGMT_FUNCS, ""
#define MAGIC_MEMPOOL_RESET_FUNC_NAMES      MAGIC_MEMPOOL_RESET_FUNCS, ""
#define MAGIC_MEMPOOL_FUNC_NAMES            MAGIC_MEMPOOL_FUNCS, ""

/* Flags for inlining wrapper calls. */
#define MAGIC_PRE_HOOK_SIMPLE_CALL          0x0001
#define MAGIC_PRE_HOOK_FORWARDING_CALL      0x0002
#define MAGIC_POST_HOOK_SIMPLE_CALL         0x0004
#define MAGIC_POST_HOOK_FORWARDING_CALL     0x0008
#define MAGIC_PRE_HOOK_DEBUG                0x0010
#define MAGIC_POST_HOOK_DEBUG               0x0020
#define MAGIC_PRE_HOOK_FLAGS_MASK                                              \
    (MAGIC_PRE_HOOK_SIMPLE_CALL | MAGIC_PRE_HOOK_FORWARDING_CALL)
#define MAGIC_POST_HOOK_FLAGS_MASK                                             \
    (MAGIC_POST_HOOK_SIMPLE_CALL | MAGIC_POST_HOOK_FORWARDING_CALL)
#define MAGIC_HOOK_DEBUG_MASK                                                  \
    (MAGIC_PRE_HOOK_DEBUG | MAGIC_POST_HOOK_DEBUG)

#if (MAGIC_MEM_USAGE_OUTPUT_CTL == 1)
#define MAGIC_MEMPOOL_CREATE_FUNC_FLAGS  MAGIC_PRE_HOOK_SIMPLE_CALL | MAGIC_POST_HOOK_FORWARDING_CALL | MAGIC_PRE_HOOK_DEBUG
#else
#define MAGIC_MEMPOOL_CREATE_FUNC_FLAGS  MAGIC_PRE_HOOK_SIMPLE_CALL | MAGIC_POST_HOOK_FORWARDING_CALL
#endif
#define MAGIC_MEMPOOL_DESTROY_FUNC_FLAGS                                       \
    MAGIC_PRE_HOOK_FORWARDING_CALL | MAGIC_POST_HOOK_SIMPLE_CALL
#define MAGIC_MEMPOOL_MGMT_FUNC_FLAGS                                          \
    MAGIC_PRE_HOOK_FORWARDING_CALL | MAGIC_POST_HOOK_SIMPLE_CALL
#define MAGIC_MEMPOOL_RESET_FUNC_FLAGS                                         \
    MAGIC_PRE_HOOK_FORWARDING_CALL | MAGIC_POST_HOOK_SIMPLE_CALL
#define MAGIC_MEMPOOL_FUNC_FLAGS                                               \
    MAGIC_MEMPOOL_CREATE_FUNC_FLAGS,                                           \
    MAGIC_MEMPOOL_DESTROY_FUNC_FLAGS,                                          \
    MAGIC_MEMPOOL_MGMT_FUNC_FLAGS,                                             \
    MAGIC_MEMPOOL_RESET_FUNC_FLAGS

#define MAGIC_CHECKPOINT_ENABLED            "_magic_checkpoint_enabled"
#define MAGIC_CHECKPOINT_FUNC_NAME          "sef_receive_status"

#define MAGIC_SHADOW_FUNC_PREFIX            "llvm_shadow"

#define MAGIC_LAZY_CHECKPOINT_ENABLED       "_magic_lazy_checkpoint_enabled"
#define MAGIC_LAZY_CHECKPOINT_CLEARDF_FUNC_NAME "sef_receive_status"

#define MAGIC_LAZY_CHECKPOINT_SHADOW_TAG    "llvm_shadow"

/* Magic memory functions. */
#define MAGIC_MEMA_FUNCS                                                       \
    __X(malloc), __X(calloc), __X(realloc),                                    \
    __X(posix_memalign), __X(valloc), __X(memalign),                           \
    __X(mmap),                                                                 \
    __X(brk), __X(sbrk),                                                       \
    __X(shmat),                                                                \
    __X(mmap64)
#define MAGIC_MEMA_FUNC_ALLOC_FLAGS                                            \
    MAGIC_STATE_HEAP, MAGIC_STATE_HEAP, MAGIC_STATE_HEAP,                      \
    MAGIC_STATE_HEAP, MAGIC_STATE_HEAP, MAGIC_STATE_HEAP,                      \
    MAGIC_STATE_MAP,                                                           \
    MAGIC_STATE_HEAP, MAGIC_STATE_HEAP,                                        \
    MAGIC_STATE_SHM,                                                           \
    MAGIC_STATE_MAP

#ifdef __MINIX
#define MAGIC_MEMA_EXTRA_FUNCS , __X(vm_map_cacheblock)
#define MAGIC_MEMA_EXTRA_FUNC_ALLOC_FLAGS , MAGIC_STATE_MAP
#else
#define MAGIC_MEMA_EXTRA_FUNCS
#define MAGIC_MEMA_EXTRA_FUNC_ALLOC_FLAGS
#endif

#define MAGIC_MEMD_FUNCS                                                       \
    __X(free), __X(munmap), __X(shmdt)
#define MAGIC_MEMD_FUNC_ALLOC_FLAGS                                            \
    0, 0, 0

#define MAGIC_MEM_FUNCS                     MAGIC_MEMA_FUNCS MAGIC_MEMA_EXTRA_FUNCS, MAGIC_MEMD_FUNCS
#define MAGIC_MEM_UNS_FUNCS                                                    \
    __X(mmap2), __X(remap_file_pages), __X(mremap)
#define MAGIC_MEM_FUNC_ALLOC_FLAGS                                             \
    MAGIC_MEMA_FUNC_ALLOC_FLAGS MAGIC_MEMA_EXTRA_FUNC_ALLOC_FLAGS, MAGIC_MEMD_FUNC_ALLOC_FLAGS

#ifdef __MINIX
/* Nested allocation functions to hook. That is, functions that are being
 * called as part of allocation functions - in particular, malloc - and need to
 * be intercepted for tracking purposes - in particular, so that mmap'ed malloc
 * page directories can be unmapped in order to avoid memory leaks. MINIX3 only.
 */
#define MAGIC_MEMN_FUNCS                                                       \
    __X(mmap), __X(munmap)
#else
#define MAGIC_MEMN_FUNCS ""
#endif

#define MAGIC_DL_FUNCS                                                         \
   __X(dlopen), __X(dlclose)

#define MAGIC_MEMA_FUNC_NAMES               MAGIC_MEMA_FUNCS MAGIC_MEMA_EXTRA_FUNCS, ""
#define MAGIC_MEMD_FUNC_NAMES               MAGIC_MEMD_FUNCS, ""
#define MAGIC_MEMN_FUNC_NAMES               MAGIC_MEMN_FUNCS, ""
#define MAGIC_MEM_FUNC_NAMES                MAGIC_MEM_FUNCS, ""
#define MAGIC_DL_FUNC_NAMES                 MAGIC_DL_FUNCS, ""

#if MAGIC_INSTRUMENT_MEM_FUNCS_ASR_ONLY
#define MAGIC_MEM_PREFIX_STRS    MAGIC_ASR_PREFIX_STR, MAGIC_PREFIX_STR, ""
#else
#define MAGIC_MEM_PREFIX_STRS               MAGIC_PREFIX_STR, ""
#endif

#endif /* _MAGIC_COMMON_H */


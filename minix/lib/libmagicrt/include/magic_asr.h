#include <magic_def.h>
#include <magic.h>
#include <magic_mem.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

PUBLIC int magic_asr_get_padding_size(int region);
PUBLIC void magic_asr_permute_dsentries(struct _magic_dsentry
    **first_dsentry_ptr);
PUBLIC void magic_asr_init(void);

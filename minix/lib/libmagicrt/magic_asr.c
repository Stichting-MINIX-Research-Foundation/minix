#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <magic_def.h>
#include <magic_mem.h>
#include <magic_asr.h>
#include <magic.h>

#ifdef __MINIX
static unsigned long magic_rand_next;
static void magic_srand(unsigned int seed)
{
    magic_rand_next = (unsigned long) seed;
}

static int magic_rand()
{
    magic_rand_next = magic_rand_next * 1103515245 + 12345;
    return (int)(magic_rand_next % ((unsigned long)RAND_MAX + 1));
}
static int magic_rand_seed()
{
    int x;
    return (int)&x + (int)&magic_rand_seed;
}
#else
#define magic_srand     srand
#define magic_rand      rand
#define magic_rand_seed() time(0)
#endif

#define MINIMUM_PADDING 1

PUBLIC int magic_asr_get_padding_size(int region) {
    int padding = 0;

    switch(region) {
        case MAGIC_STATE_HEAP | MAGIC_ASR_FLAG_INIT:
            if(_magic_asr_heap_max_offset){
                padding = (magic_rand() % _magic_asr_heap_max_offset) + MINIMUM_PADDING;
            }
            break;
        case MAGIC_STATE_HEAP:
            if(_magic_asr_heap_max_padding){
                 padding = (magic_rand() % _magic_asr_heap_max_padding) + MINIMUM_PADDING;
            }
            break;
        case MAGIC_STATE_MAP | MAGIC_ASR_FLAG_INIT:
            if(_magic_asr_map_max_offset_pages){
                padding = ((magic_rand() % _magic_asr_map_max_offset_pages) + MINIMUM_PADDING) * magic_get_sys_pagesize();
            }
            break;
        case MAGIC_STATE_MAP:
            if(_magic_asr_map_max_padding_pages){
                padding = ((magic_rand() % _magic_asr_map_max_padding_pages) + MINIMUM_PADDING) * magic_get_sys_pagesize();
            }
            break;
        default:
            padding = -1;
    }
    return padding;
}

PUBLIC void magic_asr_permute_dsentries(struct _magic_dsentry **first_dsentry_ptr){
    struct _magic_dsentry *first_dsentry = *first_dsentry_ptr, *dsentry = first_dsentry, *last_dsentry;
    int n_dsentries = 0;
    int i;

    if(!_magic_asr_heap_map_do_permutate){
        /*
         * Dsentries order is reversed anyway, because newer dsentries are
         * placed at the start of the linked list, instead of the end
         */
        return;
    }

    while(dsentry != NULL){
        last_dsentry = dsentry;
        n_dsentries++;
        dsentry = dsentry->next;
    }

    for(i=0; i < n_dsentries; i++){
        int j;
        int pos = magic_rand() % (n_dsentries - i);
        struct _magic_dsentry *prev_dsentry = NULL;

        if((i == 0) && (pos == (n_dsentries -1))){
            /*
             * Rest of for-loop won't function correctly when last dsentry is chosen first.
             * Instead, nothing has to be done in this case.
             */
            continue;
        }

        dsentry = first_dsentry;

        for(j=0;j<pos;j++){
            prev_dsentry = dsentry;
            dsentry = dsentry->next;
        }

        if(pos == 0){
            first_dsentry = first_dsentry->next;
        }else{
            prev_dsentry->next = dsentry->next;
        }

        dsentry->next = NULL;
        last_dsentry->next = dsentry;
        last_dsentry = dsentry;
    }
    *first_dsentry_ptr = first_dsentry;
}

PUBLIC void magic_asr_init(){
    int seed, heap_offset;
    if(_magic_asr_seed){
        seed = _magic_asr_seed;
    }else{
        seed = magic_rand_seed();
    }
    magic_srand(seed);

    heap_offset = magic_asr_get_padding_size(MAGIC_STATE_HEAP|MAGIC_ASR_FLAG_INIT);
    if(heap_offset){
        sbrk(heap_offset);
    }
}

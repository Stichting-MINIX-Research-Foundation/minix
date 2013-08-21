/* Copyright (C) 2001-2004 Bart Massey and Jamey Sharp.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the names of the authors or their
 * institutions shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the authors.
 */

/* A generic implementation of a list of void-pointers. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "xcb.h"
#include "xcbint.h"

typedef struct node {
    struct node *next;
    unsigned int key;
    void *data;
} node;

struct _xcb_map {
    node *head;
    node **tail;
};

/* Private interface */

_xcb_map *_xcb_map_new()
{
    _xcb_map *list;
    list = malloc(sizeof(_xcb_map));
    if(!list)
        return 0;
    list->head = 0;
    list->tail = &list->head;
    return list;
}

void _xcb_map_delete(_xcb_map *list, xcb_list_free_func_t do_free)
{
    if(!list)
        return;
    while(list->head)
    {
        node *cur = list->head;
        if(do_free)
            do_free(cur->data);
        list->head = cur->next;
        free(cur);
    }
    free(list);
}

int _xcb_map_put(_xcb_map *list, unsigned int key, void *data)
{
    node *cur = malloc(sizeof(node));
    if(!cur)
        return 0;
    cur->key = key;
    cur->data = data;
    cur->next = 0;
    *list->tail = cur;
    list->tail = &cur->next;
    return 1;
}

void *_xcb_map_remove(_xcb_map *list, unsigned int key)
{
    node **cur;
    for(cur = &list->head; *cur; cur = &(*cur)->next)
        if((*cur)->key == key)
        {
            node *tmp = *cur;
            void *ret = (*cur)->data;
            *cur = (*cur)->next;
            if(!*cur)
                list->tail = cur;

            free(tmp);
            return ret;
        }
    return 0;
}

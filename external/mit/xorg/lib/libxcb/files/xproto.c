/*
 * This file generated automatically from xproto.xml by c_client.py.
 * Edit at your peril.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>  /* for offsetof() */
#include "xcbext.h"
#include "xproto.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)


/*****************************************************************************
 **
 ** void xcb_char2b_next
 ** 
 ** @param xcb_char2b_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_char2b_next (xcb_char2b_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_char2b_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_char2b_end
 ** 
 ** @param xcb_char2b_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_char2b_end (xcb_char2b_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_window_next
 ** 
 ** @param xcb_window_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_window_next (xcb_window_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_window_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_window_end
 ** 
 ** @param xcb_window_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_window_end (xcb_window_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_pixmap_next
 ** 
 ** @param xcb_pixmap_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_pixmap_next (xcb_pixmap_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_pixmap_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_pixmap_end
 ** 
 ** @param xcb_pixmap_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_pixmap_end (xcb_pixmap_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_cursor_next
 ** 
 ** @param xcb_cursor_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_cursor_next (xcb_cursor_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_cursor_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_cursor_end
 ** 
 ** @param xcb_cursor_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_cursor_end (xcb_cursor_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_font_next
 ** 
 ** @param xcb_font_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_font_next (xcb_font_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_font_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_font_end
 ** 
 ** @param xcb_font_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_font_end (xcb_font_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_gcontext_next
 ** 
 ** @param xcb_gcontext_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_gcontext_next (xcb_gcontext_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_gcontext_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_gcontext_end
 ** 
 ** @param xcb_gcontext_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_gcontext_end (xcb_gcontext_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_colormap_next
 ** 
 ** @param xcb_colormap_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_colormap_next (xcb_colormap_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_colormap_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_colormap_end
 ** 
 ** @param xcb_colormap_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_colormap_end (xcb_colormap_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_atom_next
 ** 
 ** @param xcb_atom_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_atom_next (xcb_atom_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_atom_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_atom_end
 ** 
 ** @param xcb_atom_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_atom_end (xcb_atom_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_drawable_next
 ** 
 ** @param xcb_drawable_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_drawable_next (xcb_drawable_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_drawable_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_drawable_end
 ** 
 ** @param xcb_drawable_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_drawable_end (xcb_drawable_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_fontable_next
 ** 
 ** @param xcb_fontable_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_fontable_next (xcb_fontable_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_fontable_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_fontable_end
 ** 
 ** @param xcb_fontable_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_fontable_end (xcb_fontable_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_visualid_next
 ** 
 ** @param xcb_visualid_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_visualid_next (xcb_visualid_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_visualid_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_visualid_end
 ** 
 ** @param xcb_visualid_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_visualid_end (xcb_visualid_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_timestamp_next
 ** 
 ** @param xcb_timestamp_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_timestamp_next (xcb_timestamp_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_timestamp_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_timestamp_end
 ** 
 ** @param xcb_timestamp_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_timestamp_end (xcb_timestamp_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_keysym_next
 ** 
 ** @param xcb_keysym_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_keysym_next (xcb_keysym_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_keysym_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_keysym_end
 ** 
 ** @param xcb_keysym_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_keysym_end (xcb_keysym_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_keycode_next
 ** 
 ** @param xcb_keycode_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_keycode_next (xcb_keycode_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_keycode_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_keycode_end
 ** 
 ** @param xcb_keycode_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_keycode_end (xcb_keycode_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_button_next
 ** 
 ** @param xcb_button_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_button_next (xcb_button_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_button_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_button_end
 ** 
 ** @param xcb_button_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_button_end (xcb_button_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_point_next
 ** 
 ** @param xcb_point_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_point_next (xcb_point_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_point_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_point_end
 ** 
 ** @param xcb_point_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_point_end (xcb_point_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_rectangle_next
 ** 
 ** @param xcb_rectangle_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_rectangle_next (xcb_rectangle_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_rectangle_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_rectangle_end
 ** 
 ** @param xcb_rectangle_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_rectangle_end (xcb_rectangle_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_arc_next
 ** 
 ** @param xcb_arc_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_arc_next (xcb_arc_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_arc_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_arc_end
 ** 
 ** @param xcb_arc_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_arc_end (xcb_arc_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_format_next
 ** 
 ** @param xcb_format_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_format_next (xcb_format_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_format_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_format_end
 ** 
 ** @param xcb_format_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_format_end (xcb_format_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_visualtype_next
 ** 
 ** @param xcb_visualtype_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_visualtype_next (xcb_visualtype_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_visualtype_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_visualtype_end
 ** 
 ** @param xcb_visualtype_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_visualtype_end (xcb_visualtype_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_depth_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_depth_t *_aux = (xcb_depth_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_depth_t);
    xcb_tmp += xcb_block_len;
    /* visuals */
    xcb_block_len += _aux->visuals_len * sizeof(xcb_visualtype_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_visualtype_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_visualtype_t * xcb_depth_visuals
 ** 
 ** @param const xcb_depth_t *R
 ** @returns xcb_visualtype_t *
 **
 *****************************************************************************/
 
xcb_visualtype_t *
xcb_depth_visuals (const xcb_depth_t *R  /**< */)
{
    return (xcb_visualtype_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_depth_visuals_length
 ** 
 ** @param const xcb_depth_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_depth_visuals_length (const xcb_depth_t *R  /**< */)
{
    return R->visuals_len;
}


/*****************************************************************************
 **
 ** xcb_visualtype_iterator_t xcb_depth_visuals_iterator
 ** 
 ** @param const xcb_depth_t *R
 ** @returns xcb_visualtype_iterator_t
 **
 *****************************************************************************/
 
xcb_visualtype_iterator_t
xcb_depth_visuals_iterator (const xcb_depth_t *R  /**< */)
{
    xcb_visualtype_iterator_t i;
    i.data = (xcb_visualtype_t *) (R + 1);
    i.rem = R->visuals_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_depth_next
 ** 
 ** @param xcb_depth_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_depth_next (xcb_depth_iterator_t *i  /**< */)
{
    xcb_depth_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_depth_t *)(((char *)R) + xcb_depth_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_depth_t *) child.data;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_depth_end
 ** 
 ** @param xcb_depth_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_depth_end (xcb_depth_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_depth_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_screen_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_screen_t *_aux = (xcb_screen_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_screen_t);
    xcb_tmp += xcb_block_len;
    /* allowed_depths */
    for(i=0; i<_aux->allowed_depths_len; i++) {
        xcb_tmp_len = xcb_depth_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_depth_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** int xcb_screen_allowed_depths_length
 ** 
 ** @param const xcb_screen_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_screen_allowed_depths_length (const xcb_screen_t *R  /**< */)
{
    return R->allowed_depths_len;
}


/*****************************************************************************
 **
 ** xcb_depth_iterator_t xcb_screen_allowed_depths_iterator
 ** 
 ** @param const xcb_screen_t *R
 ** @returns xcb_depth_iterator_t
 **
 *****************************************************************************/
 
xcb_depth_iterator_t
xcb_screen_allowed_depths_iterator (const xcb_screen_t *R  /**< */)
{
    xcb_depth_iterator_t i;
    i.data = (xcb_depth_t *) (R + 1);
    i.rem = R->allowed_depths_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_screen_next
 ** 
 ** @param xcb_screen_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_screen_next (xcb_screen_iterator_t *i  /**< */)
{
    xcb_screen_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_screen_t *)(((char *)R) + xcb_screen_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_screen_t *) child.data;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_screen_end
 ** 
 ** @param xcb_screen_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_screen_end (xcb_screen_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_screen_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_setup_request_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_setup_request_t *_aux = (xcb_setup_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_setup_request_t);
    xcb_tmp += xcb_block_len;
    /* authorization_protocol_name */
    xcb_block_len += _aux->authorization_protocol_name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* authorization_protocol_data */
    xcb_block_len += _aux->authorization_protocol_data_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** char * xcb_setup_request_authorization_protocol_name
 ** 
 ** @param const xcb_setup_request_t *R
 ** @returns char *
 **
 *****************************************************************************/
 
char *
xcb_setup_request_authorization_protocol_name (const xcb_setup_request_t *R  /**< */)
{
    return (char *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_setup_request_authorization_protocol_name_length
 ** 
 ** @param const xcb_setup_request_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_setup_request_authorization_protocol_name_length (const xcb_setup_request_t *R  /**< */)
{
    return R->authorization_protocol_name_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_request_authorization_protocol_name_end
 ** 
 ** @param const xcb_setup_request_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_request_authorization_protocol_name_end (const xcb_setup_request_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->authorization_protocol_name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** char * xcb_setup_request_authorization_protocol_data
 ** 
 ** @param const xcb_setup_request_t *R
 ** @returns char *
 **
 *****************************************************************************/
 
char *
xcb_setup_request_authorization_protocol_data (const xcb_setup_request_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_setup_request_authorization_protocol_name_end(R);
    return (char *) ((char *) prev.data + XCB_TYPE_PAD(char, prev.index) + 0);
}


/*****************************************************************************
 **
 ** int xcb_setup_request_authorization_protocol_data_length
 ** 
 ** @param const xcb_setup_request_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_setup_request_authorization_protocol_data_length (const xcb_setup_request_t *R  /**< */)
{
    return R->authorization_protocol_data_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_request_authorization_protocol_data_end
 ** 
 ** @param const xcb_setup_request_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_request_authorization_protocol_data_end (const xcb_setup_request_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_setup_request_authorization_protocol_name_end(R);
    i.data = ((char *) child.data) + (R->authorization_protocol_data_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_setup_request_next
 ** 
 ** @param xcb_setup_request_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_setup_request_next (xcb_setup_request_iterator_t *i  /**< */)
{
    xcb_setup_request_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_setup_request_t *)(((char *)R) + xcb_setup_request_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_setup_request_t *) child.data;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_request_end
 ** 
 ** @param xcb_setup_request_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_request_end (xcb_setup_request_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_setup_request_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_setup_failed_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_setup_failed_t *_aux = (xcb_setup_failed_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_setup_failed_t);
    xcb_tmp += xcb_block_len;
    /* reason */
    xcb_block_len += _aux->reason_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** char * xcb_setup_failed_reason
 ** 
 ** @param const xcb_setup_failed_t *R
 ** @returns char *
 **
 *****************************************************************************/
 
char *
xcb_setup_failed_reason (const xcb_setup_failed_t *R  /**< */)
{
    return (char *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_setup_failed_reason_length
 ** 
 ** @param const xcb_setup_failed_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_setup_failed_reason_length (const xcb_setup_failed_t *R  /**< */)
{
    return R->reason_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_failed_reason_end
 ** 
 ** @param const xcb_setup_failed_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_failed_reason_end (const xcb_setup_failed_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->reason_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_setup_failed_next
 ** 
 ** @param xcb_setup_failed_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_setup_failed_next (xcb_setup_failed_iterator_t *i  /**< */)
{
    xcb_setup_failed_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_setup_failed_t *)(((char *)R) + xcb_setup_failed_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_setup_failed_t *) child.data;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_failed_end
 ** 
 ** @param xcb_setup_failed_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_failed_end (xcb_setup_failed_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_setup_failed_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_setup_authenticate_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_setup_authenticate_t *_aux = (xcb_setup_authenticate_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_setup_authenticate_t);
    xcb_tmp += xcb_block_len;
    /* reason */
    xcb_block_len += (_aux->length * 4) * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** char * xcb_setup_authenticate_reason
 ** 
 ** @param const xcb_setup_authenticate_t *R
 ** @returns char *
 **
 *****************************************************************************/
 
char *
xcb_setup_authenticate_reason (const xcb_setup_authenticate_t *R  /**< */)
{
    return (char *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_setup_authenticate_reason_length
 ** 
 ** @param const xcb_setup_authenticate_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_setup_authenticate_reason_length (const xcb_setup_authenticate_t *R  /**< */)
{
    return (R->length * 4);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_authenticate_reason_end
 ** 
 ** @param const xcb_setup_authenticate_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_authenticate_reason_end (const xcb_setup_authenticate_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_setup_authenticate_next
 ** 
 ** @param xcb_setup_authenticate_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_setup_authenticate_next (xcb_setup_authenticate_iterator_t *i  /**< */)
{
    xcb_setup_authenticate_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_setup_authenticate_t *)(((char *)R) + xcb_setup_authenticate_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_setup_authenticate_t *) child.data;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_authenticate_end
 ** 
 ** @param xcb_setup_authenticate_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_authenticate_end (xcb_setup_authenticate_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_setup_authenticate_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_setup_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_setup_t *_aux = (xcb_setup_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_setup_t);
    xcb_tmp += xcb_block_len;
    /* vendor */
    xcb_block_len += _aux->vendor_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* pixmap_formats */
    xcb_block_len += _aux->pixmap_formats_len * sizeof(xcb_format_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_format_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* roots */
    for(i=0; i<_aux->roots_len; i++) {
        xcb_tmp_len = xcb_screen_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_screen_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** char * xcb_setup_vendor
 ** 
 ** @param const xcb_setup_t *R
 ** @returns char *
 **
 *****************************************************************************/
 
char *
xcb_setup_vendor (const xcb_setup_t *R  /**< */)
{
    return (char *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_setup_vendor_length
 ** 
 ** @param const xcb_setup_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_setup_vendor_length (const xcb_setup_t *R  /**< */)
{
    return R->vendor_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_vendor_end
 ** 
 ** @param const xcb_setup_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_vendor_end (const xcb_setup_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->vendor_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_format_t * xcb_setup_pixmap_formats
 ** 
 ** @param const xcb_setup_t *R
 ** @returns xcb_format_t *
 **
 *****************************************************************************/
 
xcb_format_t *
xcb_setup_pixmap_formats (const xcb_setup_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_setup_vendor_end(R);
    return (xcb_format_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_format_t, prev.index) + 0);
}


/*****************************************************************************
 **
 ** int xcb_setup_pixmap_formats_length
 ** 
 ** @param const xcb_setup_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_setup_pixmap_formats_length (const xcb_setup_t *R  /**< */)
{
    return R->pixmap_formats_len;
}


/*****************************************************************************
 **
 ** xcb_format_iterator_t xcb_setup_pixmap_formats_iterator
 ** 
 ** @param const xcb_setup_t *R
 ** @returns xcb_format_iterator_t
 **
 *****************************************************************************/
 
xcb_format_iterator_t
xcb_setup_pixmap_formats_iterator (const xcb_setup_t *R  /**< */)
{
    xcb_format_iterator_t i;
    xcb_generic_iterator_t prev = xcb_setup_vendor_end(R);
    i.data = (xcb_format_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_format_t, prev.index));
    i.rem = R->pixmap_formats_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** int xcb_setup_roots_length
 ** 
 ** @param const xcb_setup_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_setup_roots_length (const xcb_setup_t *R  /**< */)
{
    return R->roots_len;
}


/*****************************************************************************
 **
 ** xcb_screen_iterator_t xcb_setup_roots_iterator
 ** 
 ** @param const xcb_setup_t *R
 ** @returns xcb_screen_iterator_t
 **
 *****************************************************************************/
 
xcb_screen_iterator_t
xcb_setup_roots_iterator (const xcb_setup_t *R  /**< */)
{
    xcb_screen_iterator_t i;
    xcb_generic_iterator_t prev = xcb_format_end(xcb_setup_pixmap_formats_iterator(R));
    i.data = (xcb_screen_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_screen_t, prev.index));
    i.rem = R->roots_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_setup_next
 ** 
 ** @param xcb_setup_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_setup_next (xcb_setup_iterator_t *i  /**< */)
{
    xcb_setup_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_setup_t *)(((char *)R) + xcb_setup_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_setup_t *) child.data;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_setup_end
 ** 
 ** @param xcb_setup_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_setup_end (xcb_setup_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_setup_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_client_message_data_next
 ** 
 ** @param xcb_client_message_data_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_client_message_data_next (xcb_client_message_data_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_client_message_data_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_client_message_data_end
 ** 
 ** @param xcb_client_message_data_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_client_message_data_end (xcb_client_message_data_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_create_window_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_create_window_request_t *_aux = (xcb_create_window_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_create_window_request_t);
    xcb_tmp += xcb_block_len;
    /* value_list */
    xcb_block_len += xcb_popcount(_aux->value_mask) * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_window_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           depth
 ** @param xcb_window_t      wid
 ** @param xcb_window_t      parent
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @param uint16_t          border_width
 ** @param uint16_t          _class
 ** @param xcb_visualid_t    visual
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_window_checked (xcb_connection_t *c  /**< */,
                           uint8_t           depth  /**< */,
                           xcb_window_t      wid  /**< */,
                           xcb_window_t      parent  /**< */,
                           int16_t           x  /**< */,
                           int16_t           y  /**< */,
                           uint16_t          width  /**< */,
                           uint16_t          height  /**< */,
                           uint16_t          border_width  /**< */,
                           uint16_t          _class  /**< */,
                           xcb_visualid_t    visual  /**< */,
                           uint32_t          value_mask  /**< */,
                           const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_create_window_request_t xcb_out;
    
    xcb_out.depth = depth;
    xcb_out.wid = wid;
    xcb_out.parent = parent;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.border_width = border_width;
    xcb_out._class = _class;
    xcb_out.visual = visual;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_window
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           depth
 ** @param xcb_window_t      wid
 ** @param xcb_window_t      parent
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @param uint16_t          border_width
 ** @param uint16_t          _class
 ** @param xcb_visualid_t    visual
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_window (xcb_connection_t *c  /**< */,
                   uint8_t           depth  /**< */,
                   xcb_window_t      wid  /**< */,
                   xcb_window_t      parent  /**< */,
                   int16_t           x  /**< */,
                   int16_t           y  /**< */,
                   uint16_t          width  /**< */,
                   uint16_t          height  /**< */,
                   uint16_t          border_width  /**< */,
                   uint16_t          _class  /**< */,
                   xcb_visualid_t    visual  /**< */,
                   uint32_t          value_mask  /**< */,
                   const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_create_window_request_t xcb_out;
    
    xcb_out.depth = depth;
    xcb_out.wid = wid;
    xcb_out.parent = parent;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.border_width = border_width;
    xcb_out._class = _class;
    xcb_out.visual = visual;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_change_window_attributes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_change_window_attributes_request_t *_aux = (xcb_change_window_attributes_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_change_window_attributes_request_t);
    xcb_tmp += xcb_block_len;
    /* value_list */
    xcb_block_len += xcb_popcount(_aux->value_mask) * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_window_attributes_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_window_attributes_checked (xcb_connection_t *c  /**< */,
                                      xcb_window_t      window  /**< */,
                                      uint32_t          value_mask  /**< */,
                                      const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_WINDOW_ATTRIBUTES,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_window_attributes_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_window_attributes
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_window_attributes (xcb_connection_t *c  /**< */,
                              xcb_window_t      window  /**< */,
                              uint32_t          value_mask  /**< */,
                              const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_WINDOW_ATTRIBUTES,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_window_attributes_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_window_attributes_cookie_t xcb_get_window_attributes
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_get_window_attributes_cookie_t
 **
 *****************************************************************************/
 
xcb_get_window_attributes_cookie_t
xcb_get_window_attributes (xcb_connection_t *c  /**< */,
                           xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_WINDOW_ATTRIBUTES,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_window_attributes_cookie_t xcb_ret;
    xcb_get_window_attributes_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_window_attributes_cookie_t xcb_get_window_attributes_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_get_window_attributes_cookie_t
 **
 *****************************************************************************/
 
xcb_get_window_attributes_cookie_t
xcb_get_window_attributes_unchecked (xcb_connection_t *c  /**< */,
                                     xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_WINDOW_ATTRIBUTES,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_window_attributes_cookie_t xcb_ret;
    xcb_get_window_attributes_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_window_attributes_reply_t * xcb_get_window_attributes_reply
 ** 
 ** @param xcb_connection_t                    *c
 ** @param xcb_get_window_attributes_cookie_t   cookie
 ** @param xcb_generic_error_t                **e
 ** @returns xcb_get_window_attributes_reply_t *
 **
 *****************************************************************************/
 
xcb_get_window_attributes_reply_t *
xcb_get_window_attributes_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_get_window_attributes_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_get_window_attributes_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_destroy_window_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_destroy_window_checked (xcb_connection_t *c  /**< */,
                            xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_DESTROY_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_destroy_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_destroy_window
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_destroy_window (xcb_connection_t *c  /**< */,
                    xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_DESTROY_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_destroy_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_destroy_subwindows_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_destroy_subwindows_checked (xcb_connection_t *c  /**< */,
                                xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_DESTROY_SUBWINDOWS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_destroy_subwindows_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_destroy_subwindows
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_destroy_subwindows (xcb_connection_t *c  /**< */,
                        xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_DESTROY_SUBWINDOWS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_destroy_subwindows_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_save_set_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_save_set_checked (xcb_connection_t *c  /**< */,
                             uint8_t           mode  /**< */,
                             xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_SAVE_SET,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_change_save_set_request_t xcb_out;
    
    xcb_out.mode = mode;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_save_set
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_save_set (xcb_connection_t *c  /**< */,
                     uint8_t           mode  /**< */,
                     xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_SAVE_SET,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_change_save_set_request_t xcb_out;
    
    xcb_out.mode = mode;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_reparent_window_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param xcb_window_t      parent
 ** @param int16_t           x
 ** @param int16_t           y
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_reparent_window_checked (xcb_connection_t *c  /**< */,
                             xcb_window_t      window  /**< */,
                             xcb_window_t      parent  /**< */,
                             int16_t           x  /**< */,
                             int16_t           y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_REPARENT_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_reparent_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.parent = parent;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_reparent_window
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param xcb_window_t      parent
 ** @param int16_t           x
 ** @param int16_t           y
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_reparent_window (xcb_connection_t *c  /**< */,
                     xcb_window_t      window  /**< */,
                     xcb_window_t      parent  /**< */,
                     int16_t           x  /**< */,
                     int16_t           y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_REPARENT_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_reparent_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.parent = parent;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_map_window_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_map_window_checked (xcb_connection_t *c  /**< */,
                        xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_MAP_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_map_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_map_window
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_map_window (xcb_connection_t *c  /**< */,
                xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_MAP_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_map_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_map_subwindows_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_map_subwindows_checked (xcb_connection_t *c  /**< */,
                            xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_MAP_SUBWINDOWS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_map_subwindows_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_map_subwindows
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_map_subwindows (xcb_connection_t *c  /**< */,
                    xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_MAP_SUBWINDOWS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_map_subwindows_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_unmap_window_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_unmap_window_checked (xcb_connection_t *c  /**< */,
                          xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNMAP_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_unmap_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_unmap_window
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_unmap_window (xcb_connection_t *c  /**< */,
                  xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNMAP_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_unmap_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_unmap_subwindows_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_unmap_subwindows_checked (xcb_connection_t *c  /**< */,
                              xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNMAP_SUBWINDOWS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_unmap_subwindows_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_unmap_subwindows
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_unmap_subwindows (xcb_connection_t *c  /**< */,
                      xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNMAP_SUBWINDOWS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_unmap_subwindows_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_configure_window_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_configure_window_request_t *_aux = (xcb_configure_window_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_configure_window_request_t);
    xcb_tmp += xcb_block_len;
    /* value_list */
    xcb_block_len += xcb_popcount(_aux->value_mask) * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_configure_window_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param uint16_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_configure_window_checked (xcb_connection_t *c  /**< */,
                              xcb_window_t      window  /**< */,
                              uint16_t          value_mask  /**< */,
                              const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CONFIGURE_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_configure_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.value_mask = value_mask;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_configure_window
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param uint16_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_configure_window (xcb_connection_t *c  /**< */,
                      xcb_window_t      window  /**< */,
                      uint16_t          value_mask  /**< */,
                      const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CONFIGURE_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_configure_window_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.value_mask = value_mask;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_circulate_window_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           direction
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_circulate_window_checked (xcb_connection_t *c  /**< */,
                              uint8_t           direction  /**< */,
                              xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CIRCULATE_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_circulate_window_request_t xcb_out;
    
    xcb_out.direction = direction;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_circulate_window
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           direction
 ** @param xcb_window_t      window
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_circulate_window (xcb_connection_t *c  /**< */,
                      uint8_t           direction  /**< */,
                      xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CIRCULATE_WINDOW,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_circulate_window_request_t xcb_out;
    
    xcb_out.direction = direction;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_geometry_cookie_t xcb_get_geometry
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @returns xcb_get_geometry_cookie_t
 **
 *****************************************************************************/
 
xcb_get_geometry_cookie_t
xcb_get_geometry (xcb_connection_t *c  /**< */,
                  xcb_drawable_t    drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_GEOMETRY,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_geometry_cookie_t xcb_ret;
    xcb_get_geometry_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_geometry_cookie_t xcb_get_geometry_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @returns xcb_get_geometry_cookie_t
 **
 *****************************************************************************/
 
xcb_get_geometry_cookie_t
xcb_get_geometry_unchecked (xcb_connection_t *c  /**< */,
                            xcb_drawable_t    drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_GEOMETRY,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_geometry_cookie_t xcb_ret;
    xcb_get_geometry_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_geometry_reply_t * xcb_get_geometry_reply
 ** 
 ** @param xcb_connection_t           *c
 ** @param xcb_get_geometry_cookie_t   cookie
 ** @param xcb_generic_error_t       **e
 ** @returns xcb_get_geometry_reply_t *
 **
 *****************************************************************************/
 
xcb_get_geometry_reply_t *
xcb_get_geometry_reply (xcb_connection_t           *c  /**< */,
                        xcb_get_geometry_cookie_t   cookie  /**< */,
                        xcb_generic_error_t       **e  /**< */)
{
    return (xcb_get_geometry_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_query_tree_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_query_tree_reply_t *_aux = (xcb_query_tree_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_query_tree_reply_t);
    xcb_tmp += xcb_block_len;
    /* children */
    xcb_block_len += _aux->children_len * sizeof(xcb_window_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_window_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_query_tree_cookie_t xcb_query_tree
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_query_tree_cookie_t
 **
 *****************************************************************************/
 
xcb_query_tree_cookie_t
xcb_query_tree (xcb_connection_t *c  /**< */,
                xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_TREE,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_tree_cookie_t xcb_ret;
    xcb_query_tree_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_tree_cookie_t xcb_query_tree_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_query_tree_cookie_t
 **
 *****************************************************************************/
 
xcb_query_tree_cookie_t
xcb_query_tree_unchecked (xcb_connection_t *c  /**< */,
                          xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_TREE,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_tree_cookie_t xcb_ret;
    xcb_query_tree_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_window_t * xcb_query_tree_children
 ** 
 ** @param const xcb_query_tree_reply_t *R
 ** @returns xcb_window_t *
 **
 *****************************************************************************/
 
xcb_window_t *
xcb_query_tree_children (const xcb_query_tree_reply_t *R  /**< */)
{
    return (xcb_window_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_query_tree_children_length
 ** 
 ** @param const xcb_query_tree_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_query_tree_children_length (const xcb_query_tree_reply_t *R  /**< */)
{
    return R->children_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_query_tree_children_end
 ** 
 ** @param const xcb_query_tree_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_query_tree_children_end (const xcb_query_tree_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_window_t *) (R + 1)) + (R->children_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_query_tree_reply_t * xcb_query_tree_reply
 ** 
 ** @param xcb_connection_t         *c
 ** @param xcb_query_tree_cookie_t   cookie
 ** @param xcb_generic_error_t     **e
 ** @returns xcb_query_tree_reply_t *
 **
 *****************************************************************************/
 
xcb_query_tree_reply_t *
xcb_query_tree_reply (xcb_connection_t         *c  /**< */,
                      xcb_query_tree_cookie_t   cookie  /**< */,
                      xcb_generic_error_t     **e  /**< */)
{
    return (xcb_query_tree_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_intern_atom_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_intern_atom_request_t *_aux = (xcb_intern_atom_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_intern_atom_request_t);
    xcb_tmp += xcb_block_len;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_intern_atom_cookie_t xcb_intern_atom
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           only_if_exists
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_intern_atom_cookie_t
 **
 *****************************************************************************/
 
xcb_intern_atom_cookie_t
xcb_intern_atom (xcb_connection_t *c  /**< */,
                 uint8_t           only_if_exists  /**< */,
                 uint16_t          name_len  /**< */,
                 const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_INTERN_ATOM,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_intern_atom_cookie_t xcb_ret;
    xcb_intern_atom_request_t xcb_out;
    
    xcb_out.only_if_exists = only_if_exists;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_intern_atom_cookie_t xcb_intern_atom_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           only_if_exists
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_intern_atom_cookie_t
 **
 *****************************************************************************/
 
xcb_intern_atom_cookie_t
xcb_intern_atom_unchecked (xcb_connection_t *c  /**< */,
                           uint8_t           only_if_exists  /**< */,
                           uint16_t          name_len  /**< */,
                           const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_INTERN_ATOM,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_intern_atom_cookie_t xcb_ret;
    xcb_intern_atom_request_t xcb_out;
    
    xcb_out.only_if_exists = only_if_exists;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_intern_atom_reply_t * xcb_intern_atom_reply
 ** 
 ** @param xcb_connection_t          *c
 ** @param xcb_intern_atom_cookie_t   cookie
 ** @param xcb_generic_error_t      **e
 ** @returns xcb_intern_atom_reply_t *
 **
 *****************************************************************************/
 
xcb_intern_atom_reply_t *
xcb_intern_atom_reply (xcb_connection_t          *c  /**< */,
                       xcb_intern_atom_cookie_t   cookie  /**< */,
                       xcb_generic_error_t      **e  /**< */)
{
    return (xcb_intern_atom_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_get_atom_name_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_get_atom_name_reply_t *_aux = (xcb_get_atom_name_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_get_atom_name_reply_t);
    xcb_tmp += xcb_block_len;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_get_atom_name_cookie_t xcb_get_atom_name
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_atom_t        atom
 ** @returns xcb_get_atom_name_cookie_t
 **
 *****************************************************************************/
 
xcb_get_atom_name_cookie_t
xcb_get_atom_name (xcb_connection_t *c  /**< */,
                   xcb_atom_t        atom  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_ATOM_NAME,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_atom_name_cookie_t xcb_ret;
    xcb_get_atom_name_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.atom = atom;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_atom_name_cookie_t xcb_get_atom_name_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_atom_t        atom
 ** @returns xcb_get_atom_name_cookie_t
 **
 *****************************************************************************/
 
xcb_get_atom_name_cookie_t
xcb_get_atom_name_unchecked (xcb_connection_t *c  /**< */,
                             xcb_atom_t        atom  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_ATOM_NAME,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_atom_name_cookie_t xcb_ret;
    xcb_get_atom_name_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.atom = atom;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** char * xcb_get_atom_name_name
 ** 
 ** @param const xcb_get_atom_name_reply_t *R
 ** @returns char *
 **
 *****************************************************************************/
 
char *
xcb_get_atom_name_name (const xcb_get_atom_name_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_get_atom_name_name_length
 ** 
 ** @param const xcb_get_atom_name_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_get_atom_name_name_length (const xcb_get_atom_name_reply_t *R  /**< */)
{
    return R->name_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_get_atom_name_name_end
 ** 
 ** @param const xcb_get_atom_name_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_get_atom_name_name_end (const xcb_get_atom_name_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_get_atom_name_reply_t * xcb_get_atom_name_reply
 ** 
 ** @param xcb_connection_t            *c
 ** @param xcb_get_atom_name_cookie_t   cookie
 ** @param xcb_generic_error_t        **e
 ** @returns xcb_get_atom_name_reply_t *
 **
 *****************************************************************************/
 
xcb_get_atom_name_reply_t *
xcb_get_atom_name_reply (xcb_connection_t            *c  /**< */,
                         xcb_get_atom_name_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_get_atom_name_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_change_property_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_change_property_request_t *_aux = (xcb_change_property_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_change_property_request_t);
    xcb_tmp += xcb_block_len;
    /* data */
    xcb_block_len += ((_aux->data_len * _aux->format) / 8) * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_property_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @param xcb_window_t      window
 ** @param xcb_atom_t        property
 ** @param xcb_atom_t        type
 ** @param uint8_t           format
 ** @param uint32_t          data_len
 ** @param const void       *data
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_property_checked (xcb_connection_t *c  /**< */,
                             uint8_t           mode  /**< */,
                             xcb_window_t      window  /**< */,
                             xcb_atom_t        property  /**< */,
                             xcb_atom_t        type  /**< */,
                             uint8_t           format  /**< */,
                             uint32_t          data_len  /**< */,
                             const void       *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_PROPERTY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_property_request_t xcb_out;
    
    xcb_out.mode = mode;
    xcb_out.window = window;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.format = format;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.data_len = data_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* void data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = ((data_len * format) / 8) * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_property
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @param xcb_window_t      window
 ** @param xcb_atom_t        property
 ** @param xcb_atom_t        type
 ** @param uint8_t           format
 ** @param uint32_t          data_len
 ** @param const void       *data
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_property (xcb_connection_t *c  /**< */,
                     uint8_t           mode  /**< */,
                     xcb_window_t      window  /**< */,
                     xcb_atom_t        property  /**< */,
                     xcb_atom_t        type  /**< */,
                     uint8_t           format  /**< */,
                     uint32_t          data_len  /**< */,
                     const void       *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_PROPERTY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_property_request_t xcb_out;
    
    xcb_out.mode = mode;
    xcb_out.window = window;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.format = format;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.data_len = data_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* void data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = ((data_len * format) / 8) * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_delete_property_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param xcb_atom_t        property
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_delete_property_checked (xcb_connection_t *c  /**< */,
                             xcb_window_t      window  /**< */,
                             xcb_atom_t        property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_DELETE_PROPERTY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_delete_property_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.property = property;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_delete_property
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param xcb_atom_t        property
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_delete_property (xcb_connection_t *c  /**< */,
                     xcb_window_t      window  /**< */,
                     xcb_atom_t        property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_DELETE_PROPERTY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_delete_property_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.property = property;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_get_property_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_get_property_reply_t *_aux = (xcb_get_property_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_get_property_reply_t);
    xcb_tmp += xcb_block_len;
    /* value */
    xcb_block_len += (_aux->value_len * (_aux->format / 8)) * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_get_property_cookie_t xcb_get_property
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           _delete
 ** @param xcb_window_t      window
 ** @param xcb_atom_t        property
 ** @param xcb_atom_t        type
 ** @param uint32_t          long_offset
 ** @param uint32_t          long_length
 ** @returns xcb_get_property_cookie_t
 **
 *****************************************************************************/
 
xcb_get_property_cookie_t
xcb_get_property (xcb_connection_t *c  /**< */,
                  uint8_t           _delete  /**< */,
                  xcb_window_t      window  /**< */,
                  xcb_atom_t        property  /**< */,
                  xcb_atom_t        type  /**< */,
                  uint32_t          long_offset  /**< */,
                  uint32_t          long_length  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_PROPERTY,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_property_cookie_t xcb_ret;
    xcb_get_property_request_t xcb_out;
    
    xcb_out._delete = _delete;
    xcb_out.window = window;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.long_offset = long_offset;
    xcb_out.long_length = long_length;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_property_cookie_t xcb_get_property_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           _delete
 ** @param xcb_window_t      window
 ** @param xcb_atom_t        property
 ** @param xcb_atom_t        type
 ** @param uint32_t          long_offset
 ** @param uint32_t          long_length
 ** @returns xcb_get_property_cookie_t
 **
 *****************************************************************************/
 
xcb_get_property_cookie_t
xcb_get_property_unchecked (xcb_connection_t *c  /**< */,
                            uint8_t           _delete  /**< */,
                            xcb_window_t      window  /**< */,
                            xcb_atom_t        property  /**< */,
                            xcb_atom_t        type  /**< */,
                            uint32_t          long_offset  /**< */,
                            uint32_t          long_length  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_PROPERTY,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_property_cookie_t xcb_ret;
    xcb_get_property_request_t xcb_out;
    
    xcb_out._delete = _delete;
    xcb_out.window = window;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.long_offset = long_offset;
    xcb_out.long_length = long_length;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** void * xcb_get_property_value
 ** 
 ** @param const xcb_get_property_reply_t *R
 ** @returns void *
 **
 *****************************************************************************/
 
void *
xcb_get_property_value (const xcb_get_property_reply_t *R  /**< */)
{
    return (void *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_get_property_value_length
 ** 
 ** @param const xcb_get_property_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_get_property_value_length (const xcb_get_property_reply_t *R  /**< */)
{
    return (R->value_len * (R->format / 8));
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_get_property_value_end
 ** 
 ** @param const xcb_get_property_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_get_property_value_end (const xcb_get_property_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + ((R->value_len * (R->format / 8)));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_get_property_reply_t * xcb_get_property_reply
 ** 
 ** @param xcb_connection_t           *c
 ** @param xcb_get_property_cookie_t   cookie
 ** @param xcb_generic_error_t       **e
 ** @returns xcb_get_property_reply_t *
 **
 *****************************************************************************/
 
xcb_get_property_reply_t *
xcb_get_property_reply (xcb_connection_t           *c  /**< */,
                        xcb_get_property_cookie_t   cookie  /**< */,
                        xcb_generic_error_t       **e  /**< */)
{
    return (xcb_get_property_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_list_properties_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_list_properties_reply_t *_aux = (xcb_list_properties_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_list_properties_reply_t);
    xcb_tmp += xcb_block_len;
    /* atoms */
    xcb_block_len += _aux->atoms_len * sizeof(xcb_atom_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_atom_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_list_properties_cookie_t xcb_list_properties
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_list_properties_cookie_t
 **
 *****************************************************************************/
 
xcb_list_properties_cookie_t
xcb_list_properties (xcb_connection_t *c  /**< */,
                     xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_LIST_PROPERTIES,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_list_properties_cookie_t xcb_ret;
    xcb_list_properties_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_list_properties_cookie_t xcb_list_properties_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_list_properties_cookie_t
 **
 *****************************************************************************/
 
xcb_list_properties_cookie_t
xcb_list_properties_unchecked (xcb_connection_t *c  /**< */,
                               xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_LIST_PROPERTIES,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_list_properties_cookie_t xcb_ret;
    xcb_list_properties_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_atom_t * xcb_list_properties_atoms
 ** 
 ** @param const xcb_list_properties_reply_t *R
 ** @returns xcb_atom_t *
 **
 *****************************************************************************/
 
xcb_atom_t *
xcb_list_properties_atoms (const xcb_list_properties_reply_t *R  /**< */)
{
    return (xcb_atom_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_list_properties_atoms_length
 ** 
 ** @param const xcb_list_properties_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_list_properties_atoms_length (const xcb_list_properties_reply_t *R  /**< */)
{
    return R->atoms_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_list_properties_atoms_end
 ** 
 ** @param const xcb_list_properties_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_list_properties_atoms_end (const xcb_list_properties_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_atom_t *) (R + 1)) + (R->atoms_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_list_properties_reply_t * xcb_list_properties_reply
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_list_properties_cookie_t   cookie
 ** @param xcb_generic_error_t          **e
 ** @returns xcb_list_properties_reply_t *
 **
 *****************************************************************************/
 
xcb_list_properties_reply_t *
xcb_list_properties_reply (xcb_connection_t              *c  /**< */,
                           xcb_list_properties_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_list_properties_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_selection_owner_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      owner
 ** @param xcb_atom_t        selection
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_selection_owner_checked (xcb_connection_t *c  /**< */,
                                 xcb_window_t      owner  /**< */,
                                 xcb_atom_t        selection  /**< */,
                                 xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_SELECTION_OWNER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_selection_owner_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.owner = owner;
    xcb_out.selection = selection;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_selection_owner
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      owner
 ** @param xcb_atom_t        selection
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_selection_owner (xcb_connection_t *c  /**< */,
                         xcb_window_t      owner  /**< */,
                         xcb_atom_t        selection  /**< */,
                         xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_SELECTION_OWNER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_selection_owner_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.owner = owner;
    xcb_out.selection = selection;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_selection_owner_cookie_t xcb_get_selection_owner
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_atom_t        selection
 ** @returns xcb_get_selection_owner_cookie_t
 **
 *****************************************************************************/
 
xcb_get_selection_owner_cookie_t
xcb_get_selection_owner (xcb_connection_t *c  /**< */,
                         xcb_atom_t        selection  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_SELECTION_OWNER,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_selection_owner_cookie_t xcb_ret;
    xcb_get_selection_owner_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.selection = selection;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_selection_owner_cookie_t xcb_get_selection_owner_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_atom_t        selection
 ** @returns xcb_get_selection_owner_cookie_t
 **
 *****************************************************************************/
 
xcb_get_selection_owner_cookie_t
xcb_get_selection_owner_unchecked (xcb_connection_t *c  /**< */,
                                   xcb_atom_t        selection  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_SELECTION_OWNER,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_selection_owner_cookie_t xcb_ret;
    xcb_get_selection_owner_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.selection = selection;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_selection_owner_reply_t * xcb_get_selection_owner_reply
 ** 
 ** @param xcb_connection_t                  *c
 ** @param xcb_get_selection_owner_cookie_t   cookie
 ** @param xcb_generic_error_t              **e
 ** @returns xcb_get_selection_owner_reply_t *
 **
 *****************************************************************************/
 
xcb_get_selection_owner_reply_t *
xcb_get_selection_owner_reply (xcb_connection_t                  *c  /**< */,
                               xcb_get_selection_owner_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_get_selection_owner_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_convert_selection_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      requestor
 ** @param xcb_atom_t        selection
 ** @param xcb_atom_t        target
 ** @param xcb_atom_t        property
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_convert_selection_checked (xcb_connection_t *c  /**< */,
                               xcb_window_t      requestor  /**< */,
                               xcb_atom_t        selection  /**< */,
                               xcb_atom_t        target  /**< */,
                               xcb_atom_t        property  /**< */,
                               xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CONVERT_SELECTION,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_convert_selection_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.requestor = requestor;
    xcb_out.selection = selection;
    xcb_out.target = target;
    xcb_out.property = property;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_convert_selection
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      requestor
 ** @param xcb_atom_t        selection
 ** @param xcb_atom_t        target
 ** @param xcb_atom_t        property
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_convert_selection (xcb_connection_t *c  /**< */,
                       xcb_window_t      requestor  /**< */,
                       xcb_atom_t        selection  /**< */,
                       xcb_atom_t        target  /**< */,
                       xcb_atom_t        property  /**< */,
                       xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CONVERT_SELECTION,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_convert_selection_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.requestor = requestor;
    xcb_out.selection = selection;
    xcb_out.target = target;
    xcb_out.property = property;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_send_event_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           propagate
 ** @param xcb_window_t      destination
 ** @param uint32_t          event_mask
 ** @param const char       *event
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_send_event_checked (xcb_connection_t *c  /**< */,
                        uint8_t           propagate  /**< */,
                        xcb_window_t      destination  /**< */,
                        uint32_t          event_mask  /**< */,
                        const char       *event  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SEND_EVENT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_send_event_request_t xcb_out;
    
    xcb_out.propagate = propagate;
    xcb_out.destination = destination;
    xcb_out.event_mask = event_mask;
    memcpy(xcb_out.event, event, 32);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_send_event
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           propagate
 ** @param xcb_window_t      destination
 ** @param uint32_t          event_mask
 ** @param const char       *event
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_send_event (xcb_connection_t *c  /**< */,
                uint8_t           propagate  /**< */,
                xcb_window_t      destination  /**< */,
                uint32_t          event_mask  /**< */,
                const char       *event  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SEND_EVENT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_send_event_request_t xcb_out;
    
    xcb_out.propagate = propagate;
    xcb_out.destination = destination;
    xcb_out.event_mask = event_mask;
    memcpy(xcb_out.event, event, 32);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_grab_pointer_cookie_t xcb_grab_pointer
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           owner_events
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          event_mask
 ** @param uint8_t           pointer_mode
 ** @param uint8_t           keyboard_mode
 ** @param xcb_window_t      confine_to
 ** @param xcb_cursor_t      cursor
 ** @param xcb_timestamp_t   time
 ** @returns xcb_grab_pointer_cookie_t
 **
 *****************************************************************************/
 
xcb_grab_pointer_cookie_t
xcb_grab_pointer (xcb_connection_t *c  /**< */,
                  uint8_t           owner_events  /**< */,
                  xcb_window_t      grab_window  /**< */,
                  uint16_t          event_mask  /**< */,
                  uint8_t           pointer_mode  /**< */,
                  uint8_t           keyboard_mode  /**< */,
                  xcb_window_t      confine_to  /**< */,
                  xcb_cursor_t      cursor  /**< */,
                  xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_POINTER,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_grab_pointer_cookie_t xcb_ret;
    xcb_grab_pointer_request_t xcb_out;
    
    xcb_out.owner_events = owner_events;
    xcb_out.grab_window = grab_window;
    xcb_out.event_mask = event_mask;
    xcb_out.pointer_mode = pointer_mode;
    xcb_out.keyboard_mode = keyboard_mode;
    xcb_out.confine_to = confine_to;
    xcb_out.cursor = cursor;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_grab_pointer_cookie_t xcb_grab_pointer_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           owner_events
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          event_mask
 ** @param uint8_t           pointer_mode
 ** @param uint8_t           keyboard_mode
 ** @param xcb_window_t      confine_to
 ** @param xcb_cursor_t      cursor
 ** @param xcb_timestamp_t   time
 ** @returns xcb_grab_pointer_cookie_t
 **
 *****************************************************************************/
 
xcb_grab_pointer_cookie_t
xcb_grab_pointer_unchecked (xcb_connection_t *c  /**< */,
                            uint8_t           owner_events  /**< */,
                            xcb_window_t      grab_window  /**< */,
                            uint16_t          event_mask  /**< */,
                            uint8_t           pointer_mode  /**< */,
                            uint8_t           keyboard_mode  /**< */,
                            xcb_window_t      confine_to  /**< */,
                            xcb_cursor_t      cursor  /**< */,
                            xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_POINTER,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_grab_pointer_cookie_t xcb_ret;
    xcb_grab_pointer_request_t xcb_out;
    
    xcb_out.owner_events = owner_events;
    xcb_out.grab_window = grab_window;
    xcb_out.event_mask = event_mask;
    xcb_out.pointer_mode = pointer_mode;
    xcb_out.keyboard_mode = keyboard_mode;
    xcb_out.confine_to = confine_to;
    xcb_out.cursor = cursor;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_grab_pointer_reply_t * xcb_grab_pointer_reply
 ** 
 ** @param xcb_connection_t           *c
 ** @param xcb_grab_pointer_cookie_t   cookie
 ** @param xcb_generic_error_t       **e
 ** @returns xcb_grab_pointer_reply_t *
 **
 *****************************************************************************/
 
xcb_grab_pointer_reply_t *
xcb_grab_pointer_reply (xcb_connection_t           *c  /**< */,
                        xcb_grab_pointer_cookie_t   cookie  /**< */,
                        xcb_generic_error_t       **e  /**< */)
{
    return (xcb_grab_pointer_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_pointer_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_pointer_checked (xcb_connection_t *c  /**< */,
                            xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_POINTER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_pointer_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_pointer
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_pointer (xcb_connection_t *c  /**< */,
                    xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_POINTER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_pointer_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_grab_button_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           owner_events
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          event_mask
 ** @param uint8_t           pointer_mode
 ** @param uint8_t           keyboard_mode
 ** @param xcb_window_t      confine_to
 ** @param xcb_cursor_t      cursor
 ** @param uint8_t           button
 ** @param uint16_t          modifiers
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_grab_button_checked (xcb_connection_t *c  /**< */,
                         uint8_t           owner_events  /**< */,
                         xcb_window_t      grab_window  /**< */,
                         uint16_t          event_mask  /**< */,
                         uint8_t           pointer_mode  /**< */,
                         uint8_t           keyboard_mode  /**< */,
                         xcb_window_t      confine_to  /**< */,
                         xcb_cursor_t      cursor  /**< */,
                         uint8_t           button  /**< */,
                         uint16_t          modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_BUTTON,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_grab_button_request_t xcb_out;
    
    xcb_out.owner_events = owner_events;
    xcb_out.grab_window = grab_window;
    xcb_out.event_mask = event_mask;
    xcb_out.pointer_mode = pointer_mode;
    xcb_out.keyboard_mode = keyboard_mode;
    xcb_out.confine_to = confine_to;
    xcb_out.cursor = cursor;
    xcb_out.button = button;
    xcb_out.pad0 = 0;
    xcb_out.modifiers = modifiers;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_grab_button
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           owner_events
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          event_mask
 ** @param uint8_t           pointer_mode
 ** @param uint8_t           keyboard_mode
 ** @param xcb_window_t      confine_to
 ** @param xcb_cursor_t      cursor
 ** @param uint8_t           button
 ** @param uint16_t          modifiers
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_grab_button (xcb_connection_t *c  /**< */,
                 uint8_t           owner_events  /**< */,
                 xcb_window_t      grab_window  /**< */,
                 uint16_t          event_mask  /**< */,
                 uint8_t           pointer_mode  /**< */,
                 uint8_t           keyboard_mode  /**< */,
                 xcb_window_t      confine_to  /**< */,
                 xcb_cursor_t      cursor  /**< */,
                 uint8_t           button  /**< */,
                 uint16_t          modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_BUTTON,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_grab_button_request_t xcb_out;
    
    xcb_out.owner_events = owner_events;
    xcb_out.grab_window = grab_window;
    xcb_out.event_mask = event_mask;
    xcb_out.pointer_mode = pointer_mode;
    xcb_out.keyboard_mode = keyboard_mode;
    xcb_out.confine_to = confine_to;
    xcb_out.cursor = cursor;
    xcb_out.button = button;
    xcb_out.pad0 = 0;
    xcb_out.modifiers = modifiers;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_button_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           button
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_button_checked (xcb_connection_t *c  /**< */,
                           uint8_t           button  /**< */,
                           xcb_window_t      grab_window  /**< */,
                           uint16_t          modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_BUTTON,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_button_request_t xcb_out;
    
    xcb_out.button = button;
    xcb_out.grab_window = grab_window;
    xcb_out.modifiers = modifiers;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_button
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           button
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_button (xcb_connection_t *c  /**< */,
                   uint8_t           button  /**< */,
                   xcb_window_t      grab_window  /**< */,
                   uint16_t          modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_BUTTON,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_button_request_t xcb_out;
    
    xcb_out.button = button;
    xcb_out.grab_window = grab_window;
    xcb_out.modifiers = modifiers;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_active_pointer_grab_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cursor
 ** @param xcb_timestamp_t   time
 ** @param uint16_t          event_mask
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_active_pointer_grab_checked (xcb_connection_t *c  /**< */,
                                        xcb_cursor_t      cursor  /**< */,
                                        xcb_timestamp_t   time  /**< */,
                                        uint16_t          event_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_ACTIVE_POINTER_GRAB,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_change_active_pointer_grab_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cursor = cursor;
    xcb_out.time = time;
    xcb_out.event_mask = event_mask;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_active_pointer_grab
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cursor
 ** @param xcb_timestamp_t   time
 ** @param uint16_t          event_mask
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_active_pointer_grab (xcb_connection_t *c  /**< */,
                                xcb_cursor_t      cursor  /**< */,
                                xcb_timestamp_t   time  /**< */,
                                uint16_t          event_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_ACTIVE_POINTER_GRAB,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_change_active_pointer_grab_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cursor = cursor;
    xcb_out.time = time;
    xcb_out.event_mask = event_mask;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_grab_keyboard_cookie_t xcb_grab_keyboard
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           owner_events
 ** @param xcb_window_t      grab_window
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           pointer_mode
 ** @param uint8_t           keyboard_mode
 ** @returns xcb_grab_keyboard_cookie_t
 **
 *****************************************************************************/
 
xcb_grab_keyboard_cookie_t
xcb_grab_keyboard (xcb_connection_t *c  /**< */,
                   uint8_t           owner_events  /**< */,
                   xcb_window_t      grab_window  /**< */,
                   xcb_timestamp_t   time  /**< */,
                   uint8_t           pointer_mode  /**< */,
                   uint8_t           keyboard_mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_KEYBOARD,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_grab_keyboard_cookie_t xcb_ret;
    xcb_grab_keyboard_request_t xcb_out;
    
    xcb_out.owner_events = owner_events;
    xcb_out.grab_window = grab_window;
    xcb_out.time = time;
    xcb_out.pointer_mode = pointer_mode;
    xcb_out.keyboard_mode = keyboard_mode;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_grab_keyboard_cookie_t xcb_grab_keyboard_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           owner_events
 ** @param xcb_window_t      grab_window
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           pointer_mode
 ** @param uint8_t           keyboard_mode
 ** @returns xcb_grab_keyboard_cookie_t
 **
 *****************************************************************************/
 
xcb_grab_keyboard_cookie_t
xcb_grab_keyboard_unchecked (xcb_connection_t *c  /**< */,
                             uint8_t           owner_events  /**< */,
                             xcb_window_t      grab_window  /**< */,
                             xcb_timestamp_t   time  /**< */,
                             uint8_t           pointer_mode  /**< */,
                             uint8_t           keyboard_mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_KEYBOARD,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_grab_keyboard_cookie_t xcb_ret;
    xcb_grab_keyboard_request_t xcb_out;
    
    xcb_out.owner_events = owner_events;
    xcb_out.grab_window = grab_window;
    xcb_out.time = time;
    xcb_out.pointer_mode = pointer_mode;
    xcb_out.keyboard_mode = keyboard_mode;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_grab_keyboard_reply_t * xcb_grab_keyboard_reply
 ** 
 ** @param xcb_connection_t            *c
 ** @param xcb_grab_keyboard_cookie_t   cookie
 ** @param xcb_generic_error_t        **e
 ** @returns xcb_grab_keyboard_reply_t *
 **
 *****************************************************************************/
 
xcb_grab_keyboard_reply_t *
xcb_grab_keyboard_reply (xcb_connection_t            *c  /**< */,
                         xcb_grab_keyboard_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_grab_keyboard_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_keyboard_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_keyboard_checked (xcb_connection_t *c  /**< */,
                             xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_KEYBOARD,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_keyboard_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_keyboard
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_keyboard (xcb_connection_t *c  /**< */,
                     xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_KEYBOARD,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_keyboard_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_grab_key_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           owner_events
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @param xcb_keycode_t     key
 ** @param uint8_t           pointer_mode
 ** @param uint8_t           keyboard_mode
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_grab_key_checked (xcb_connection_t *c  /**< */,
                      uint8_t           owner_events  /**< */,
                      xcb_window_t      grab_window  /**< */,
                      uint16_t          modifiers  /**< */,
                      xcb_keycode_t     key  /**< */,
                      uint8_t           pointer_mode  /**< */,
                      uint8_t           keyboard_mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_KEY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_grab_key_request_t xcb_out;
    
    xcb_out.owner_events = owner_events;
    xcb_out.grab_window = grab_window;
    xcb_out.modifiers = modifiers;
    xcb_out.key = key;
    xcb_out.pointer_mode = pointer_mode;
    xcb_out.keyboard_mode = keyboard_mode;
    memset(xcb_out.pad0, 0, 3);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_grab_key
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           owner_events
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @param xcb_keycode_t     key
 ** @param uint8_t           pointer_mode
 ** @param uint8_t           keyboard_mode
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_grab_key (xcb_connection_t *c  /**< */,
              uint8_t           owner_events  /**< */,
              xcb_window_t      grab_window  /**< */,
              uint16_t          modifiers  /**< */,
              xcb_keycode_t     key  /**< */,
              uint8_t           pointer_mode  /**< */,
              uint8_t           keyboard_mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_KEY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_grab_key_request_t xcb_out;
    
    xcb_out.owner_events = owner_events;
    xcb_out.grab_window = grab_window;
    xcb_out.modifiers = modifiers;
    xcb_out.key = key;
    xcb_out.pointer_mode = pointer_mode;
    xcb_out.keyboard_mode = keyboard_mode;
    memset(xcb_out.pad0, 0, 3);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_key_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_keycode_t     key
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_key_checked (xcb_connection_t *c  /**< */,
                        xcb_keycode_t     key  /**< */,
                        xcb_window_t      grab_window  /**< */,
                        uint16_t          modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_KEY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_key_request_t xcb_out;
    
    xcb_out.key = key;
    xcb_out.grab_window = grab_window;
    xcb_out.modifiers = modifiers;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_key
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_keycode_t     key
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_key (xcb_connection_t *c  /**< */,
                xcb_keycode_t     key  /**< */,
                xcb_window_t      grab_window  /**< */,
                uint16_t          modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_KEY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_key_request_t xcb_out;
    
    xcb_out.key = key;
    xcb_out.grab_window = grab_window;
    xcb_out.modifiers = modifiers;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_allow_events_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_allow_events_checked (xcb_connection_t *c  /**< */,
                          uint8_t           mode  /**< */,
                          xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_ALLOW_EVENTS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_allow_events_request_t xcb_out;
    
    xcb_out.mode = mode;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_allow_events
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_allow_events (xcb_connection_t *c  /**< */,
                  uint8_t           mode  /**< */,
                  xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_ALLOW_EVENTS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_allow_events_request_t xcb_out;
    
    xcb_out.mode = mode;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_grab_server_checked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_grab_server_checked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_SERVER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_grab_server_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_grab_server
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_grab_server (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GRAB_SERVER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_grab_server_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_server_checked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_server_checked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_SERVER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_server_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_ungrab_server
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_ungrab_server (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNGRAB_SERVER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_ungrab_server_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_pointer_cookie_t xcb_query_pointer
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_query_pointer_cookie_t
 **
 *****************************************************************************/
 
xcb_query_pointer_cookie_t
xcb_query_pointer (xcb_connection_t *c  /**< */,
                   xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_POINTER,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_pointer_cookie_t xcb_ret;
    xcb_query_pointer_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_pointer_cookie_t xcb_query_pointer_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_query_pointer_cookie_t
 **
 *****************************************************************************/
 
xcb_query_pointer_cookie_t
xcb_query_pointer_unchecked (xcb_connection_t *c  /**< */,
                             xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_POINTER,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_pointer_cookie_t xcb_ret;
    xcb_query_pointer_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_pointer_reply_t * xcb_query_pointer_reply
 ** 
 ** @param xcb_connection_t            *c
 ** @param xcb_query_pointer_cookie_t   cookie
 ** @param xcb_generic_error_t        **e
 ** @returns xcb_query_pointer_reply_t *
 **
 *****************************************************************************/
 
xcb_query_pointer_reply_t *
xcb_query_pointer_reply (xcb_connection_t            *c  /**< */,
                         xcb_query_pointer_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_query_pointer_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** void xcb_timecoord_next
 ** 
 ** @param xcb_timecoord_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_timecoord_next (xcb_timecoord_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_timecoord_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_timecoord_end
 ** 
 ** @param xcb_timecoord_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_timecoord_end (xcb_timecoord_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_get_motion_events_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_get_motion_events_reply_t *_aux = (xcb_get_motion_events_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_get_motion_events_reply_t);
    xcb_tmp += xcb_block_len;
    /* events */
    xcb_block_len += _aux->events_len * sizeof(xcb_timecoord_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_timecoord_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_get_motion_events_cookie_t xcb_get_motion_events
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param xcb_timestamp_t   start
 ** @param xcb_timestamp_t   stop
 ** @returns xcb_get_motion_events_cookie_t
 **
 *****************************************************************************/
 
xcb_get_motion_events_cookie_t
xcb_get_motion_events (xcb_connection_t *c  /**< */,
                       xcb_window_t      window  /**< */,
                       xcb_timestamp_t   start  /**< */,
                       xcb_timestamp_t   stop  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_MOTION_EVENTS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_motion_events_cookie_t xcb_ret;
    xcb_get_motion_events_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.start = start;
    xcb_out.stop = stop;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_motion_events_cookie_t xcb_get_motion_events_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param xcb_timestamp_t   start
 ** @param xcb_timestamp_t   stop
 ** @returns xcb_get_motion_events_cookie_t
 **
 *****************************************************************************/
 
xcb_get_motion_events_cookie_t
xcb_get_motion_events_unchecked (xcb_connection_t *c  /**< */,
                                 xcb_window_t      window  /**< */,
                                 xcb_timestamp_t   start  /**< */,
                                 xcb_timestamp_t   stop  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_MOTION_EVENTS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_motion_events_cookie_t xcb_ret;
    xcb_get_motion_events_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.start = start;
    xcb_out.stop = stop;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_timecoord_t * xcb_get_motion_events_events
 ** 
 ** @param const xcb_get_motion_events_reply_t *R
 ** @returns xcb_timecoord_t *
 **
 *****************************************************************************/
 
xcb_timecoord_t *
xcb_get_motion_events_events (const xcb_get_motion_events_reply_t *R  /**< */)
{
    return (xcb_timecoord_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_get_motion_events_events_length
 ** 
 ** @param const xcb_get_motion_events_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_get_motion_events_events_length (const xcb_get_motion_events_reply_t *R  /**< */)
{
    return R->events_len;
}


/*****************************************************************************
 **
 ** xcb_timecoord_iterator_t xcb_get_motion_events_events_iterator
 ** 
 ** @param const xcb_get_motion_events_reply_t *R
 ** @returns xcb_timecoord_iterator_t
 **
 *****************************************************************************/
 
xcb_timecoord_iterator_t
xcb_get_motion_events_events_iterator (const xcb_get_motion_events_reply_t *R  /**< */)
{
    xcb_timecoord_iterator_t i;
    i.data = (xcb_timecoord_t *) (R + 1);
    i.rem = R->events_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_get_motion_events_reply_t * xcb_get_motion_events_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_get_motion_events_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_get_motion_events_reply_t *
 **
 *****************************************************************************/
 
xcb_get_motion_events_reply_t *
xcb_get_motion_events_reply (xcb_connection_t                *c  /**< */,
                             xcb_get_motion_events_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_get_motion_events_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_translate_coordinates_cookie_t xcb_translate_coordinates
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      src_window
 ** @param xcb_window_t      dst_window
 ** @param int16_t           src_x
 ** @param int16_t           src_y
 ** @returns xcb_translate_coordinates_cookie_t
 **
 *****************************************************************************/
 
xcb_translate_coordinates_cookie_t
xcb_translate_coordinates (xcb_connection_t *c  /**< */,
                           xcb_window_t      src_window  /**< */,
                           xcb_window_t      dst_window  /**< */,
                           int16_t           src_x  /**< */,
                           int16_t           src_y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_TRANSLATE_COORDINATES,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_translate_coordinates_cookie_t xcb_ret;
    xcb_translate_coordinates_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_window = src_window;
    xcb_out.dst_window = dst_window;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_translate_coordinates_cookie_t xcb_translate_coordinates_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      src_window
 ** @param xcb_window_t      dst_window
 ** @param int16_t           src_x
 ** @param int16_t           src_y
 ** @returns xcb_translate_coordinates_cookie_t
 **
 *****************************************************************************/
 
xcb_translate_coordinates_cookie_t
xcb_translate_coordinates_unchecked (xcb_connection_t *c  /**< */,
                                     xcb_window_t      src_window  /**< */,
                                     xcb_window_t      dst_window  /**< */,
                                     int16_t           src_x  /**< */,
                                     int16_t           src_y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_TRANSLATE_COORDINATES,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_translate_coordinates_cookie_t xcb_ret;
    xcb_translate_coordinates_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_window = src_window;
    xcb_out.dst_window = dst_window;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_translate_coordinates_reply_t * xcb_translate_coordinates_reply
 ** 
 ** @param xcb_connection_t                    *c
 ** @param xcb_translate_coordinates_cookie_t   cookie
 ** @param xcb_generic_error_t                **e
 ** @returns xcb_translate_coordinates_reply_t *
 **
 *****************************************************************************/
 
xcb_translate_coordinates_reply_t *
xcb_translate_coordinates_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_translate_coordinates_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_translate_coordinates_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_warp_pointer_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      src_window
 ** @param xcb_window_t      dst_window
 ** @param int16_t           src_x
 ** @param int16_t           src_y
 ** @param uint16_t          src_width
 ** @param uint16_t          src_height
 ** @param int16_t           dst_x
 ** @param int16_t           dst_y
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_warp_pointer_checked (xcb_connection_t *c  /**< */,
                          xcb_window_t      src_window  /**< */,
                          xcb_window_t      dst_window  /**< */,
                          int16_t           src_x  /**< */,
                          int16_t           src_y  /**< */,
                          uint16_t          src_width  /**< */,
                          uint16_t          src_height  /**< */,
                          int16_t           dst_x  /**< */,
                          int16_t           dst_y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_WARP_POINTER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_warp_pointer_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_window = src_window;
    xcb_out.dst_window = dst_window;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_width = src_width;
    xcb_out.src_height = src_height;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_warp_pointer
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      src_window
 ** @param xcb_window_t      dst_window
 ** @param int16_t           src_x
 ** @param int16_t           src_y
 ** @param uint16_t          src_width
 ** @param uint16_t          src_height
 ** @param int16_t           dst_x
 ** @param int16_t           dst_y
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_warp_pointer (xcb_connection_t *c  /**< */,
                  xcb_window_t      src_window  /**< */,
                  xcb_window_t      dst_window  /**< */,
                  int16_t           src_x  /**< */,
                  int16_t           src_y  /**< */,
                  uint16_t          src_width  /**< */,
                  uint16_t          src_height  /**< */,
                  int16_t           dst_x  /**< */,
                  int16_t           dst_y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_WARP_POINTER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_warp_pointer_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_window = src_window;
    xcb_out.dst_window = dst_window;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_width = src_width;
    xcb_out.src_height = src_height;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_input_focus_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           revert_to
 ** @param xcb_window_t      focus
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_input_focus_checked (xcb_connection_t *c  /**< */,
                             uint8_t           revert_to  /**< */,
                             xcb_window_t      focus  /**< */,
                             xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_INPUT_FOCUS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_input_focus_request_t xcb_out;
    
    xcb_out.revert_to = revert_to;
    xcb_out.focus = focus;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_input_focus
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           revert_to
 ** @param xcb_window_t      focus
 ** @param xcb_timestamp_t   time
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_input_focus (xcb_connection_t *c  /**< */,
                     uint8_t           revert_to  /**< */,
                     xcb_window_t      focus  /**< */,
                     xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_INPUT_FOCUS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_input_focus_request_t xcb_out;
    
    xcb_out.revert_to = revert_to;
    xcb_out.focus = focus;
    xcb_out.time = time;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_input_focus_cookie_t xcb_get_input_focus
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_input_focus_cookie_t
 **
 *****************************************************************************/
 
xcb_get_input_focus_cookie_t
xcb_get_input_focus (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_INPUT_FOCUS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_input_focus_cookie_t xcb_ret;
    xcb_get_input_focus_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_input_focus_cookie_t xcb_get_input_focus_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_input_focus_cookie_t
 **
 *****************************************************************************/
 
xcb_get_input_focus_cookie_t
xcb_get_input_focus_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_INPUT_FOCUS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_input_focus_cookie_t xcb_ret;
    xcb_get_input_focus_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_input_focus_reply_t * xcb_get_input_focus_reply
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_get_input_focus_cookie_t   cookie
 ** @param xcb_generic_error_t          **e
 ** @returns xcb_get_input_focus_reply_t *
 **
 *****************************************************************************/
 
xcb_get_input_focus_reply_t *
xcb_get_input_focus_reply (xcb_connection_t              *c  /**< */,
                           xcb_get_input_focus_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_get_input_focus_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_query_keymap_cookie_t xcb_query_keymap
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_query_keymap_cookie_t
 **
 *****************************************************************************/
 
xcb_query_keymap_cookie_t
xcb_query_keymap (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_KEYMAP,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_keymap_cookie_t xcb_ret;
    xcb_query_keymap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_keymap_cookie_t xcb_query_keymap_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_query_keymap_cookie_t
 **
 *****************************************************************************/
 
xcb_query_keymap_cookie_t
xcb_query_keymap_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_KEYMAP,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_keymap_cookie_t xcb_ret;
    xcb_query_keymap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_keymap_reply_t * xcb_query_keymap_reply
 ** 
 ** @param xcb_connection_t           *c
 ** @param xcb_query_keymap_cookie_t   cookie
 ** @param xcb_generic_error_t       **e
 ** @returns xcb_query_keymap_reply_t *
 **
 *****************************************************************************/
 
xcb_query_keymap_reply_t *
xcb_query_keymap_reply (xcb_connection_t           *c  /**< */,
                        xcb_query_keymap_cookie_t   cookie  /**< */,
                        xcb_generic_error_t       **e  /**< */)
{
    return (xcb_query_keymap_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_open_font_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_open_font_request_t *_aux = (xcb_open_font_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_open_font_request_t);
    xcb_tmp += xcb_block_len;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_open_font_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_font_t        fid
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_open_font_checked (xcb_connection_t *c  /**< */,
                       xcb_font_t        fid  /**< */,
                       uint16_t          name_len  /**< */,
                       const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_OPEN_FONT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_open_font_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.fid = fid;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_open_font
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_font_t        fid
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_open_font (xcb_connection_t *c  /**< */,
               xcb_font_t        fid  /**< */,
               uint16_t          name_len  /**< */,
               const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_OPEN_FONT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_open_font_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.fid = fid;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_close_font_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_font_t        font
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_close_font_checked (xcb_connection_t *c  /**< */,
                        xcb_font_t        font  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CLOSE_FONT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_close_font_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.font = font;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_close_font
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_font_t        font
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_close_font (xcb_connection_t *c  /**< */,
                xcb_font_t        font  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CLOSE_FONT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_close_font_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.font = font;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** void xcb_fontprop_next
 ** 
 ** @param xcb_fontprop_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_fontprop_next (xcb_fontprop_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_fontprop_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_fontprop_end
 ** 
 ** @param xcb_fontprop_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_fontprop_end (xcb_fontprop_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_charinfo_next
 ** 
 ** @param xcb_charinfo_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_charinfo_next (xcb_charinfo_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_charinfo_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_charinfo_end
 ** 
 ** @param xcb_charinfo_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_charinfo_end (xcb_charinfo_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_query_font_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_query_font_reply_t *_aux = (xcb_query_font_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_query_font_reply_t);
    xcb_tmp += xcb_block_len;
    /* properties */
    xcb_block_len += _aux->properties_len * sizeof(xcb_fontprop_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_fontprop_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* char_infos */
    xcb_block_len += _aux->char_infos_len * sizeof(xcb_charinfo_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_charinfo_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_query_font_cookie_t xcb_query_font
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_fontable_t    font
 ** @returns xcb_query_font_cookie_t
 **
 *****************************************************************************/
 
xcb_query_font_cookie_t
xcb_query_font (xcb_connection_t *c  /**< */,
                xcb_fontable_t    font  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_FONT,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_font_cookie_t xcb_ret;
    xcb_query_font_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.font = font;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_font_cookie_t xcb_query_font_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_fontable_t    font
 ** @returns xcb_query_font_cookie_t
 **
 *****************************************************************************/
 
xcb_query_font_cookie_t
xcb_query_font_unchecked (xcb_connection_t *c  /**< */,
                          xcb_fontable_t    font  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_FONT,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_font_cookie_t xcb_ret;
    xcb_query_font_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.font = font;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_fontprop_t * xcb_query_font_properties
 ** 
 ** @param const xcb_query_font_reply_t *R
 ** @returns xcb_fontprop_t *
 **
 *****************************************************************************/
 
xcb_fontprop_t *
xcb_query_font_properties (const xcb_query_font_reply_t *R  /**< */)
{
    return (xcb_fontprop_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_query_font_properties_length
 ** 
 ** @param const xcb_query_font_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_query_font_properties_length (const xcb_query_font_reply_t *R  /**< */)
{
    return R->properties_len;
}


/*****************************************************************************
 **
 ** xcb_fontprop_iterator_t xcb_query_font_properties_iterator
 ** 
 ** @param const xcb_query_font_reply_t *R
 ** @returns xcb_fontprop_iterator_t
 **
 *****************************************************************************/
 
xcb_fontprop_iterator_t
xcb_query_font_properties_iterator (const xcb_query_font_reply_t *R  /**< */)
{
    xcb_fontprop_iterator_t i;
    i.data = (xcb_fontprop_t *) (R + 1);
    i.rem = R->properties_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_charinfo_t * xcb_query_font_char_infos
 ** 
 ** @param const xcb_query_font_reply_t *R
 ** @returns xcb_charinfo_t *
 **
 *****************************************************************************/
 
xcb_charinfo_t *
xcb_query_font_char_infos (const xcb_query_font_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_fontprop_end(xcb_query_font_properties_iterator(R));
    return (xcb_charinfo_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_charinfo_t, prev.index) + 0);
}


/*****************************************************************************
 **
 ** int xcb_query_font_char_infos_length
 ** 
 ** @param const xcb_query_font_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_query_font_char_infos_length (const xcb_query_font_reply_t *R  /**< */)
{
    return R->char_infos_len;
}


/*****************************************************************************
 **
 ** xcb_charinfo_iterator_t xcb_query_font_char_infos_iterator
 ** 
 ** @param const xcb_query_font_reply_t *R
 ** @returns xcb_charinfo_iterator_t
 **
 *****************************************************************************/
 
xcb_charinfo_iterator_t
xcb_query_font_char_infos_iterator (const xcb_query_font_reply_t *R  /**< */)
{
    xcb_charinfo_iterator_t i;
    xcb_generic_iterator_t prev = xcb_fontprop_end(xcb_query_font_properties_iterator(R));
    i.data = (xcb_charinfo_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_charinfo_t, prev.index));
    i.rem = R->char_infos_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_query_font_reply_t * xcb_query_font_reply
 ** 
 ** @param xcb_connection_t         *c
 ** @param xcb_query_font_cookie_t   cookie
 ** @param xcb_generic_error_t     **e
 ** @returns xcb_query_font_reply_t *
 **
 *****************************************************************************/
 
xcb_query_font_reply_t *
xcb_query_font_reply (xcb_connection_t         *c  /**< */,
                      xcb_query_font_cookie_t   cookie  /**< */,
                      xcb_generic_error_t     **e  /**< */)
{
    return (xcb_query_font_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_query_text_extents_sizeof (const void  *_buffer  /**< */,
                               uint32_t     string_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_query_text_extents_request_t);
    xcb_tmp += xcb_block_len;
    /* string */
    xcb_block_len += string_len * sizeof(xcb_char2b_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_char2b_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_query_text_extents_cookie_t xcb_query_text_extents
 ** 
 ** @param xcb_connection_t   *c
 ** @param xcb_fontable_t      font
 ** @param uint32_t            string_len
 ** @param const xcb_char2b_t *string
 ** @returns xcb_query_text_extents_cookie_t
 **
 *****************************************************************************/
 
xcb_query_text_extents_cookie_t
xcb_query_text_extents (xcb_connection_t   *c  /**< */,
                        xcb_fontable_t      font  /**< */,
                        uint32_t            string_len  /**< */,
                        const xcb_char2b_t *string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_TEXT_EXTENTS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_query_text_extents_cookie_t xcb_ret;
    xcb_query_text_extents_request_t xcb_out;
    
    xcb_out.odd_length = (string_len & 1);
    xcb_out.font = font;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_char2b_t string */
    xcb_parts[4].iov_base = (char *) string;
    xcb_parts[4].iov_len = string_len * sizeof(xcb_char2b_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_text_extents_cookie_t xcb_query_text_extents_unchecked
 ** 
 ** @param xcb_connection_t   *c
 ** @param xcb_fontable_t      font
 ** @param uint32_t            string_len
 ** @param const xcb_char2b_t *string
 ** @returns xcb_query_text_extents_cookie_t
 **
 *****************************************************************************/
 
xcb_query_text_extents_cookie_t
xcb_query_text_extents_unchecked (xcb_connection_t   *c  /**< */,
                                  xcb_fontable_t      font  /**< */,
                                  uint32_t            string_len  /**< */,
                                  const xcb_char2b_t *string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_TEXT_EXTENTS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_query_text_extents_cookie_t xcb_ret;
    xcb_query_text_extents_request_t xcb_out;
    
    xcb_out.odd_length = (string_len & 1);
    xcb_out.font = font;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_char2b_t string */
    xcb_parts[4].iov_base = (char *) string;
    xcb_parts[4].iov_len = string_len * sizeof(xcb_char2b_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_text_extents_reply_t * xcb_query_text_extents_reply
 ** 
 ** @param xcb_connection_t                 *c
 ** @param xcb_query_text_extents_cookie_t   cookie
 ** @param xcb_generic_error_t             **e
 ** @returns xcb_query_text_extents_reply_t *
 **
 *****************************************************************************/
 
xcb_query_text_extents_reply_t *
xcb_query_text_extents_reply (xcb_connection_t                 *c  /**< */,
                              xcb_query_text_extents_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_query_text_extents_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_str_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_str_t *_aux = (xcb_str_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_str_t);
    xcb_tmp += xcb_block_len;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** char * xcb_str_name
 ** 
 ** @param const xcb_str_t *R
 ** @returns char *
 **
 *****************************************************************************/
 
char *
xcb_str_name (const xcb_str_t *R  /**< */)
{
    return (char *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_str_name_length
 ** 
 ** @param const xcb_str_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_str_name_length (const xcb_str_t *R  /**< */)
{
    return R->name_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_str_name_end
 ** 
 ** @param const xcb_str_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_str_name_end (const xcb_str_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_str_next
 ** 
 ** @param xcb_str_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_str_next (xcb_str_iterator_t *i  /**< */)
{
    xcb_str_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_str_t *)(((char *)R) + xcb_str_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_str_t *) child.data;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_str_end
 ** 
 ** @param xcb_str_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_str_end (xcb_str_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_str_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_list_fonts_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_list_fonts_request_t *_aux = (xcb_list_fonts_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_list_fonts_request_t);
    xcb_tmp += xcb_block_len;
    /* pattern */
    xcb_block_len += _aux->pattern_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_list_fonts_cookie_t xcb_list_fonts
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          max_names
 ** @param uint16_t          pattern_len
 ** @param const char       *pattern
 ** @returns xcb_list_fonts_cookie_t
 **
 *****************************************************************************/
 
xcb_list_fonts_cookie_t
xcb_list_fonts (xcb_connection_t *c  /**< */,
                uint16_t          max_names  /**< */,
                uint16_t          pattern_len  /**< */,
                const char       *pattern  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_LIST_FONTS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_list_fonts_cookie_t xcb_ret;
    xcb_list_fonts_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.max_names = max_names;
    xcb_out.pattern_len = pattern_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char pattern */
    xcb_parts[4].iov_base = (char *) pattern;
    xcb_parts[4].iov_len = pattern_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_list_fonts_cookie_t xcb_list_fonts_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          max_names
 ** @param uint16_t          pattern_len
 ** @param const char       *pattern
 ** @returns xcb_list_fonts_cookie_t
 **
 *****************************************************************************/
 
xcb_list_fonts_cookie_t
xcb_list_fonts_unchecked (xcb_connection_t *c  /**< */,
                          uint16_t          max_names  /**< */,
                          uint16_t          pattern_len  /**< */,
                          const char       *pattern  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_LIST_FONTS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_list_fonts_cookie_t xcb_ret;
    xcb_list_fonts_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.max_names = max_names;
    xcb_out.pattern_len = pattern_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char pattern */
    xcb_parts[4].iov_base = (char *) pattern;
    xcb_parts[4].iov_len = pattern_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** int xcb_list_fonts_names_length
 ** 
 ** @param const xcb_list_fonts_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_list_fonts_names_length (const xcb_list_fonts_reply_t *R  /**< */)
{
    return R->names_len;
}


/*****************************************************************************
 **
 ** xcb_str_iterator_t xcb_list_fonts_names_iterator
 ** 
 ** @param const xcb_list_fonts_reply_t *R
 ** @returns xcb_str_iterator_t
 **
 *****************************************************************************/
 
xcb_str_iterator_t
xcb_list_fonts_names_iterator (const xcb_list_fonts_reply_t *R  /**< */)
{
    xcb_str_iterator_t i;
    i.data = (xcb_str_t *) (R + 1);
    i.rem = R->names_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_list_fonts_reply_t * xcb_list_fonts_reply
 ** 
 ** @param xcb_connection_t         *c
 ** @param xcb_list_fonts_cookie_t   cookie
 ** @param xcb_generic_error_t     **e
 ** @returns xcb_list_fonts_reply_t *
 **
 *****************************************************************************/
 
xcb_list_fonts_reply_t *
xcb_list_fonts_reply (xcb_connection_t         *c  /**< */,
                      xcb_list_fonts_cookie_t   cookie  /**< */,
                      xcb_generic_error_t     **e  /**< */)
{
    return (xcb_list_fonts_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_list_fonts_with_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_list_fonts_with_info_request_t *_aux = (xcb_list_fonts_with_info_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_list_fonts_with_info_request_t);
    xcb_tmp += xcb_block_len;
    /* pattern */
    xcb_block_len += _aux->pattern_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_list_fonts_with_info_cookie_t xcb_list_fonts_with_info
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          max_names
 ** @param uint16_t          pattern_len
 ** @param const char       *pattern
 ** @returns xcb_list_fonts_with_info_cookie_t
 **
 *****************************************************************************/
 
xcb_list_fonts_with_info_cookie_t
xcb_list_fonts_with_info (xcb_connection_t *c  /**< */,
                          uint16_t          max_names  /**< */,
                          uint16_t          pattern_len  /**< */,
                          const char       *pattern  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_LIST_FONTS_WITH_INFO,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_list_fonts_with_info_cookie_t xcb_ret;
    xcb_list_fonts_with_info_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.max_names = max_names;
    xcb_out.pattern_len = pattern_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char pattern */
    xcb_parts[4].iov_base = (char *) pattern;
    xcb_parts[4].iov_len = pattern_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_list_fonts_with_info_cookie_t xcb_list_fonts_with_info_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          max_names
 ** @param uint16_t          pattern_len
 ** @param const char       *pattern
 ** @returns xcb_list_fonts_with_info_cookie_t
 **
 *****************************************************************************/
 
xcb_list_fonts_with_info_cookie_t
xcb_list_fonts_with_info_unchecked (xcb_connection_t *c  /**< */,
                                    uint16_t          max_names  /**< */,
                                    uint16_t          pattern_len  /**< */,
                                    const char       *pattern  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_LIST_FONTS_WITH_INFO,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_list_fonts_with_info_cookie_t xcb_ret;
    xcb_list_fonts_with_info_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.max_names = max_names;
    xcb_out.pattern_len = pattern_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char pattern */
    xcb_parts[4].iov_base = (char *) pattern;
    xcb_parts[4].iov_len = pattern_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_fontprop_t * xcb_list_fonts_with_info_properties
 ** 
 ** @param const xcb_list_fonts_with_info_reply_t *R
 ** @returns xcb_fontprop_t *
 **
 *****************************************************************************/
 
xcb_fontprop_t *
xcb_list_fonts_with_info_properties (const xcb_list_fonts_with_info_reply_t *R  /**< */)
{
    return (xcb_fontprop_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_list_fonts_with_info_properties_length
 ** 
 ** @param const xcb_list_fonts_with_info_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_list_fonts_with_info_properties_length (const xcb_list_fonts_with_info_reply_t *R  /**< */)
{
    return R->properties_len;
}


/*****************************************************************************
 **
 ** xcb_fontprop_iterator_t xcb_list_fonts_with_info_properties_iterator
 ** 
 ** @param const xcb_list_fonts_with_info_reply_t *R
 ** @returns xcb_fontprop_iterator_t
 **
 *****************************************************************************/
 
xcb_fontprop_iterator_t
xcb_list_fonts_with_info_properties_iterator (const xcb_list_fonts_with_info_reply_t *R  /**< */)
{
    xcb_fontprop_iterator_t i;
    i.data = (xcb_fontprop_t *) (R + 1);
    i.rem = R->properties_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** char * xcb_list_fonts_with_info_name
 ** 
 ** @param const xcb_list_fonts_with_info_reply_t *R
 ** @returns char *
 **
 *****************************************************************************/
 
char *
xcb_list_fonts_with_info_name (const xcb_list_fonts_with_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_fontprop_end(xcb_list_fonts_with_info_properties_iterator(R));
    return (char *) ((char *) prev.data + XCB_TYPE_PAD(char, prev.index) + 0);
}


/*****************************************************************************
 **
 ** int xcb_list_fonts_with_info_name_length
 ** 
 ** @param const xcb_list_fonts_with_info_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_list_fonts_with_info_name_length (const xcb_list_fonts_with_info_reply_t *R  /**< */)
{
    return R->name_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_list_fonts_with_info_name_end
 ** 
 ** @param const xcb_list_fonts_with_info_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_list_fonts_with_info_name_end (const xcb_list_fonts_with_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_fontprop_end(xcb_list_fonts_with_info_properties_iterator(R));
    i.data = ((char *) child.data) + (R->name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_list_fonts_with_info_reply_t * xcb_list_fonts_with_info_reply
 ** 
 ** @param xcb_connection_t                   *c
 ** @param xcb_list_fonts_with_info_cookie_t   cookie
 ** @param xcb_generic_error_t               **e
 ** @returns xcb_list_fonts_with_info_reply_t *
 **
 *****************************************************************************/
 
xcb_list_fonts_with_info_reply_t *
xcb_list_fonts_with_info_reply (xcb_connection_t                   *c  /**< */,
                                xcb_list_fonts_with_info_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_list_fonts_with_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_set_font_path_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_set_font_path_request_t *_aux = (xcb_set_font_path_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_set_font_path_request_t);
    xcb_tmp += xcb_block_len;
    /* font */
    for(i=0; i<_aux->font_qty; i++) {
        xcb_tmp_len = xcb_str_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_str_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_font_path_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          font_qty
 ** @param const xcb_str_t  *font
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_font_path_checked (xcb_connection_t *c  /**< */,
                           uint16_t          font_qty  /**< */,
                           const xcb_str_t  *font  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_FONT_PATH,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_set_font_path_request_t xcb_out;
    unsigned int i;
    unsigned int xcb_tmp_len;
    char *xcb_tmp;
    
    xcb_out.pad0 = 0;
    xcb_out.font_qty = font_qty;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_str_t font */
    xcb_parts[4].iov_base = (char *) font;
    xcb_parts[4].iov_len = 0;
    xcb_tmp = (char *)font;
    for(i=0; i<font_qty; i++) {
        xcb_tmp_len = xcb_str_sizeof(xcb_tmp);
        xcb_parts[4].iov_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_font_path
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          font_qty
 ** @param const xcb_str_t  *font
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_font_path (xcb_connection_t *c  /**< */,
                   uint16_t          font_qty  /**< */,
                   const xcb_str_t  *font  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_FONT_PATH,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_set_font_path_request_t xcb_out;
    unsigned int i;
    unsigned int xcb_tmp_len;
    char *xcb_tmp;
    
    xcb_out.pad0 = 0;
    xcb_out.font_qty = font_qty;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_str_t font */
    xcb_parts[4].iov_base = (char *) font;
    xcb_parts[4].iov_len = 0;
    xcb_tmp = (char *)font;
    for(i=0; i<font_qty; i++) {
        xcb_tmp_len = xcb_str_sizeof(xcb_tmp);
        xcb_parts[4].iov_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_get_font_path_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_get_font_path_reply_t *_aux = (xcb_get_font_path_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_get_font_path_reply_t);
    xcb_tmp += xcb_block_len;
    /* path */
    for(i=0; i<_aux->path_len; i++) {
        xcb_tmp_len = xcb_str_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_str_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_get_font_path_cookie_t xcb_get_font_path
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_font_path_cookie_t
 **
 *****************************************************************************/
 
xcb_get_font_path_cookie_t
xcb_get_font_path (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_FONT_PATH,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_font_path_cookie_t xcb_ret;
    xcb_get_font_path_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_font_path_cookie_t xcb_get_font_path_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_font_path_cookie_t
 **
 *****************************************************************************/
 
xcb_get_font_path_cookie_t
xcb_get_font_path_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_FONT_PATH,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_font_path_cookie_t xcb_ret;
    xcb_get_font_path_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** int xcb_get_font_path_path_length
 ** 
 ** @param const xcb_get_font_path_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_get_font_path_path_length (const xcb_get_font_path_reply_t *R  /**< */)
{
    return R->path_len;
}


/*****************************************************************************
 **
 ** xcb_str_iterator_t xcb_get_font_path_path_iterator
 ** 
 ** @param const xcb_get_font_path_reply_t *R
 ** @returns xcb_str_iterator_t
 **
 *****************************************************************************/
 
xcb_str_iterator_t
xcb_get_font_path_path_iterator (const xcb_get_font_path_reply_t *R  /**< */)
{
    xcb_str_iterator_t i;
    i.data = (xcb_str_t *) (R + 1);
    i.rem = R->path_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_get_font_path_reply_t * xcb_get_font_path_reply
 ** 
 ** @param xcb_connection_t            *c
 ** @param xcb_get_font_path_cookie_t   cookie
 ** @param xcb_generic_error_t        **e
 ** @returns xcb_get_font_path_reply_t *
 **
 *****************************************************************************/
 
xcb_get_font_path_reply_t *
xcb_get_font_path_reply (xcb_connection_t            *c  /**< */,
                         xcb_get_font_path_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_get_font_path_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_pixmap_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           depth
 ** @param xcb_pixmap_t      pid
 ** @param xcb_drawable_t    drawable
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_pixmap_checked (xcb_connection_t *c  /**< */,
                           uint8_t           depth  /**< */,
                           xcb_pixmap_t      pid  /**< */,
                           xcb_drawable_t    drawable  /**< */,
                           uint16_t          width  /**< */,
                           uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_PIXMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_create_pixmap_request_t xcb_out;
    
    xcb_out.depth = depth;
    xcb_out.pid = pid;
    xcb_out.drawable = drawable;
    xcb_out.width = width;
    xcb_out.height = height;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_pixmap
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           depth
 ** @param xcb_pixmap_t      pid
 ** @param xcb_drawable_t    drawable
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_pixmap (xcb_connection_t *c  /**< */,
                   uint8_t           depth  /**< */,
                   xcb_pixmap_t      pid  /**< */,
                   xcb_drawable_t    drawable  /**< */,
                   uint16_t          width  /**< */,
                   uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_PIXMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_create_pixmap_request_t xcb_out;
    
    xcb_out.depth = depth;
    xcb_out.pid = pid;
    xcb_out.drawable = drawable;
    xcb_out.width = width;
    xcb_out.height = height;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_pixmap_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_pixmap_t      pixmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_pixmap_checked (xcb_connection_t *c  /**< */,
                         xcb_pixmap_t      pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FREE_PIXMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_free_pixmap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.pixmap = pixmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_pixmap
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_pixmap_t      pixmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_pixmap (xcb_connection_t *c  /**< */,
                 xcb_pixmap_t      pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FREE_PIXMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_free_pixmap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.pixmap = pixmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_create_gc_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_create_gc_request_t *_aux = (xcb_create_gc_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_create_gc_request_t);
    xcb_tmp += xcb_block_len;
    /* value_list */
    xcb_block_len += xcb_popcount(_aux->value_mask) * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_gc_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    cid
 ** @param xcb_drawable_t    drawable
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_gc_checked (xcb_connection_t *c  /**< */,
                       xcb_gcontext_t    cid  /**< */,
                       xcb_drawable_t    drawable  /**< */,
                       uint32_t          value_mask  /**< */,
                       const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_GC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_create_gc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cid = cid;
    xcb_out.drawable = drawable;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_gc
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    cid
 ** @param xcb_drawable_t    drawable
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_gc (xcb_connection_t *c  /**< */,
               xcb_gcontext_t    cid  /**< */,
               xcb_drawable_t    drawable  /**< */,
               uint32_t          value_mask  /**< */,
               const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_GC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_create_gc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cid = cid;
    xcb_out.drawable = drawable;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_change_gc_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_change_gc_request_t *_aux = (xcb_change_gc_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_change_gc_request_t);
    xcb_tmp += xcb_block_len;
    /* value_list */
    xcb_block_len += xcb_popcount(_aux->value_mask) * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_gc_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    gc
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_gc_checked (xcb_connection_t *c  /**< */,
                       xcb_gcontext_t    gc  /**< */,
                       uint32_t          value_mask  /**< */,
                       const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_GC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_gc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.gc = gc;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_gc
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    gc
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_gc (xcb_connection_t *c  /**< */,
               xcb_gcontext_t    gc  /**< */,
               uint32_t          value_mask  /**< */,
               const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_GC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_gc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.gc = gc;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_copy_gc_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    src_gc
 ** @param xcb_gcontext_t    dst_gc
 ** @param uint32_t          value_mask
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_copy_gc_checked (xcb_connection_t *c  /**< */,
                     xcb_gcontext_t    src_gc  /**< */,
                     xcb_gcontext_t    dst_gc  /**< */,
                     uint32_t          value_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_COPY_GC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_copy_gc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_gc = src_gc;
    xcb_out.dst_gc = dst_gc;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_copy_gc
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    src_gc
 ** @param xcb_gcontext_t    dst_gc
 ** @param uint32_t          value_mask
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_copy_gc (xcb_connection_t *c  /**< */,
             xcb_gcontext_t    src_gc  /**< */,
             xcb_gcontext_t    dst_gc  /**< */,
             uint32_t          value_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_COPY_GC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_copy_gc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_gc = src_gc;
    xcb_out.dst_gc = dst_gc;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_set_dashes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_set_dashes_request_t *_aux = (xcb_set_dashes_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_set_dashes_request_t);
    xcb_tmp += xcb_block_len;
    /* dashes */
    xcb_block_len += _aux->dashes_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_dashes_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    gc
 ** @param uint16_t          dash_offset
 ** @param uint16_t          dashes_len
 ** @param const uint8_t    *dashes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_dashes_checked (xcb_connection_t *c  /**< */,
                        xcb_gcontext_t    gc  /**< */,
                        uint16_t          dash_offset  /**< */,
                        uint16_t          dashes_len  /**< */,
                        const uint8_t    *dashes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_DASHES,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_set_dashes_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.gc = gc;
    xcb_out.dash_offset = dash_offset;
    xcb_out.dashes_len = dashes_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t dashes */
    xcb_parts[4].iov_base = (char *) dashes;
    xcb_parts[4].iov_len = dashes_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_dashes
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    gc
 ** @param uint16_t          dash_offset
 ** @param uint16_t          dashes_len
 ** @param const uint8_t    *dashes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_dashes (xcb_connection_t *c  /**< */,
                xcb_gcontext_t    gc  /**< */,
                uint16_t          dash_offset  /**< */,
                uint16_t          dashes_len  /**< */,
                const uint8_t    *dashes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_DASHES,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_set_dashes_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.gc = gc;
    xcb_out.dash_offset = dash_offset;
    xcb_out.dashes_len = dashes_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t dashes */
    xcb_parts[4].iov_base = (char *) dashes;
    xcb_parts[4].iov_len = dashes_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_set_clip_rectangles_sizeof (const void  *_buffer  /**< */,
                                uint32_t     rectangles_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_set_clip_rectangles_request_t);
    xcb_tmp += xcb_block_len;
    /* rectangles */
    xcb_block_len += rectangles_len * sizeof(xcb_rectangle_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_rectangle_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_clip_rectangles_checked
 ** 
 ** @param xcb_connection_t      *c
 ** @param uint8_t                ordering
 ** @param xcb_gcontext_t         gc
 ** @param int16_t                clip_x_origin
 ** @param int16_t                clip_y_origin
 ** @param uint32_t               rectangles_len
 ** @param const xcb_rectangle_t *rectangles
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_clip_rectangles_checked (xcb_connection_t      *c  /**< */,
                                 uint8_t                ordering  /**< */,
                                 xcb_gcontext_t         gc  /**< */,
                                 int16_t                clip_x_origin  /**< */,
                                 int16_t                clip_y_origin  /**< */,
                                 uint32_t               rectangles_len  /**< */,
                                 const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_CLIP_RECTANGLES,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_set_clip_rectangles_request_t xcb_out;
    
    xcb_out.ordering = ordering;
    xcb_out.gc = gc;
    xcb_out.clip_x_origin = clip_x_origin;
    xcb_out.clip_y_origin = clip_y_origin;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rectangles */
    xcb_parts[4].iov_base = (char *) rectangles;
    xcb_parts[4].iov_len = rectangles_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_clip_rectangles
 ** 
 ** @param xcb_connection_t      *c
 ** @param uint8_t                ordering
 ** @param xcb_gcontext_t         gc
 ** @param int16_t                clip_x_origin
 ** @param int16_t                clip_y_origin
 ** @param uint32_t               rectangles_len
 ** @param const xcb_rectangle_t *rectangles
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_clip_rectangles (xcb_connection_t      *c  /**< */,
                         uint8_t                ordering  /**< */,
                         xcb_gcontext_t         gc  /**< */,
                         int16_t                clip_x_origin  /**< */,
                         int16_t                clip_y_origin  /**< */,
                         uint32_t               rectangles_len  /**< */,
                         const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_CLIP_RECTANGLES,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_set_clip_rectangles_request_t xcb_out;
    
    xcb_out.ordering = ordering;
    xcb_out.gc = gc;
    xcb_out.clip_x_origin = clip_x_origin;
    xcb_out.clip_y_origin = clip_y_origin;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rectangles */
    xcb_parts[4].iov_base = (char *) rectangles;
    xcb_parts[4].iov_len = rectangles_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_gc_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    gc
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_gc_checked (xcb_connection_t *c  /**< */,
                     xcb_gcontext_t    gc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FREE_GC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_free_gc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_gc
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_gcontext_t    gc
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_gc (xcb_connection_t *c  /**< */,
             xcb_gcontext_t    gc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FREE_GC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_free_gc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_clear_area_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           exposures
 ** @param xcb_window_t      window
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_clear_area_checked (xcb_connection_t *c  /**< */,
                        uint8_t           exposures  /**< */,
                        xcb_window_t      window  /**< */,
                        int16_t           x  /**< */,
                        int16_t           y  /**< */,
                        uint16_t          width  /**< */,
                        uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CLEAR_AREA,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_clear_area_request_t xcb_out;
    
    xcb_out.exposures = exposures;
    xcb_out.window = window;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_clear_area
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           exposures
 ** @param xcb_window_t      window
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_clear_area (xcb_connection_t *c  /**< */,
                uint8_t           exposures  /**< */,
                xcb_window_t      window  /**< */,
                int16_t           x  /**< */,
                int16_t           y  /**< */,
                uint16_t          width  /**< */,
                uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CLEAR_AREA,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_clear_area_request_t xcb_out;
    
    xcb_out.exposures = exposures;
    xcb_out.window = window;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_copy_area_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    src_drawable
 ** @param xcb_drawable_t    dst_drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           src_x
 ** @param int16_t           src_y
 ** @param int16_t           dst_x
 ** @param int16_t           dst_y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_copy_area_checked (xcb_connection_t *c  /**< */,
                       xcb_drawable_t    src_drawable  /**< */,
                       xcb_drawable_t    dst_drawable  /**< */,
                       xcb_gcontext_t    gc  /**< */,
                       int16_t           src_x  /**< */,
                       int16_t           src_y  /**< */,
                       int16_t           dst_x  /**< */,
                       int16_t           dst_y  /**< */,
                       uint16_t          width  /**< */,
                       uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_COPY_AREA,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_copy_area_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_drawable = src_drawable;
    xcb_out.dst_drawable = dst_drawable;
    xcb_out.gc = gc;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.width = width;
    xcb_out.height = height;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_copy_area
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    src_drawable
 ** @param xcb_drawable_t    dst_drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           src_x
 ** @param int16_t           src_y
 ** @param int16_t           dst_x
 ** @param int16_t           dst_y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_copy_area (xcb_connection_t *c  /**< */,
               xcb_drawable_t    src_drawable  /**< */,
               xcb_drawable_t    dst_drawable  /**< */,
               xcb_gcontext_t    gc  /**< */,
               int16_t           src_x  /**< */,
               int16_t           src_y  /**< */,
               int16_t           dst_x  /**< */,
               int16_t           dst_y  /**< */,
               uint16_t          width  /**< */,
               uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_COPY_AREA,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_copy_area_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_drawable = src_drawable;
    xcb_out.dst_drawable = dst_drawable;
    xcb_out.gc = gc;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.width = width;
    xcb_out.height = height;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_copy_plane_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    src_drawable
 ** @param xcb_drawable_t    dst_drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           src_x
 ** @param int16_t           src_y
 ** @param int16_t           dst_x
 ** @param int16_t           dst_y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @param uint32_t          bit_plane
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_copy_plane_checked (xcb_connection_t *c  /**< */,
                        xcb_drawable_t    src_drawable  /**< */,
                        xcb_drawable_t    dst_drawable  /**< */,
                        xcb_gcontext_t    gc  /**< */,
                        int16_t           src_x  /**< */,
                        int16_t           src_y  /**< */,
                        int16_t           dst_x  /**< */,
                        int16_t           dst_y  /**< */,
                        uint16_t          width  /**< */,
                        uint16_t          height  /**< */,
                        uint32_t          bit_plane  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_COPY_PLANE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_copy_plane_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_drawable = src_drawable;
    xcb_out.dst_drawable = dst_drawable;
    xcb_out.gc = gc;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.bit_plane = bit_plane;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_copy_plane
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    src_drawable
 ** @param xcb_drawable_t    dst_drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           src_x
 ** @param int16_t           src_y
 ** @param int16_t           dst_x
 ** @param int16_t           dst_y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @param uint32_t          bit_plane
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_copy_plane (xcb_connection_t *c  /**< */,
                xcb_drawable_t    src_drawable  /**< */,
                xcb_drawable_t    dst_drawable  /**< */,
                xcb_gcontext_t    gc  /**< */,
                int16_t           src_x  /**< */,
                int16_t           src_y  /**< */,
                int16_t           dst_x  /**< */,
                int16_t           dst_y  /**< */,
                uint16_t          width  /**< */,
                uint16_t          height  /**< */,
                uint32_t          bit_plane  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_COPY_PLANE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_copy_plane_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.src_drawable = src_drawable;
    xcb_out.dst_drawable = dst_drawable;
    xcb_out.gc = gc;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.bit_plane = bit_plane;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_poly_point_sizeof (const void  *_buffer  /**< */,
                       uint32_t     points_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_point_request_t);
    xcb_tmp += xcb_block_len;
    /* points */
    xcb_block_len += points_len * sizeof(xcb_point_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_point_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_point_checked
 ** 
 ** @param xcb_connection_t  *c
 ** @param uint8_t            coordinate_mode
 ** @param xcb_drawable_t     drawable
 ** @param xcb_gcontext_t     gc
 ** @param uint32_t           points_len
 ** @param const xcb_point_t *points
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_point_checked (xcb_connection_t  *c  /**< */,
                        uint8_t            coordinate_mode  /**< */,
                        xcb_drawable_t     drawable  /**< */,
                        xcb_gcontext_t     gc  /**< */,
                        uint32_t           points_len  /**< */,
                        const xcb_point_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_POINT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_point_request_t xcb_out;
    
    xcb_out.coordinate_mode = coordinate_mode;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_point_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_point_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_point
 ** 
 ** @param xcb_connection_t  *c
 ** @param uint8_t            coordinate_mode
 ** @param xcb_drawable_t     drawable
 ** @param xcb_gcontext_t     gc
 ** @param uint32_t           points_len
 ** @param const xcb_point_t *points
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_point (xcb_connection_t  *c  /**< */,
                uint8_t            coordinate_mode  /**< */,
                xcb_drawable_t     drawable  /**< */,
                xcb_gcontext_t     gc  /**< */,
                uint32_t           points_len  /**< */,
                const xcb_point_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_POINT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_point_request_t xcb_out;
    
    xcb_out.coordinate_mode = coordinate_mode;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_point_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_point_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_poly_line_sizeof (const void  *_buffer  /**< */,
                      uint32_t     points_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_line_request_t);
    xcb_tmp += xcb_block_len;
    /* points */
    xcb_block_len += points_len * sizeof(xcb_point_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_point_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_line_checked
 ** 
 ** @param xcb_connection_t  *c
 ** @param uint8_t            coordinate_mode
 ** @param xcb_drawable_t     drawable
 ** @param xcb_gcontext_t     gc
 ** @param uint32_t           points_len
 ** @param const xcb_point_t *points
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_line_checked (xcb_connection_t  *c  /**< */,
                       uint8_t            coordinate_mode  /**< */,
                       xcb_drawable_t     drawable  /**< */,
                       xcb_gcontext_t     gc  /**< */,
                       uint32_t           points_len  /**< */,
                       const xcb_point_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_LINE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_line_request_t xcb_out;
    
    xcb_out.coordinate_mode = coordinate_mode;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_point_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_point_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_line
 ** 
 ** @param xcb_connection_t  *c
 ** @param uint8_t            coordinate_mode
 ** @param xcb_drawable_t     drawable
 ** @param xcb_gcontext_t     gc
 ** @param uint32_t           points_len
 ** @param const xcb_point_t *points
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_line (xcb_connection_t  *c  /**< */,
               uint8_t            coordinate_mode  /**< */,
               xcb_drawable_t     drawable  /**< */,
               xcb_gcontext_t     gc  /**< */,
               uint32_t           points_len  /**< */,
               const xcb_point_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_LINE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_line_request_t xcb_out;
    
    xcb_out.coordinate_mode = coordinate_mode;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_point_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_point_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** void xcb_segment_next
 ** 
 ** @param xcb_segment_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_segment_next (xcb_segment_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_segment_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_segment_end
 ** 
 ** @param xcb_segment_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_segment_end (xcb_segment_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_poly_segment_sizeof (const void  *_buffer  /**< */,
                         uint32_t     segments_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_segment_request_t);
    xcb_tmp += xcb_block_len;
    /* segments */
    xcb_block_len += segments_len * sizeof(xcb_segment_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_segment_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_segment_checked
 ** 
 ** @param xcb_connection_t    *c
 ** @param xcb_drawable_t       drawable
 ** @param xcb_gcontext_t       gc
 ** @param uint32_t             segments_len
 ** @param const xcb_segment_t *segments
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_segment_checked (xcb_connection_t    *c  /**< */,
                          xcb_drawable_t       drawable  /**< */,
                          xcb_gcontext_t       gc  /**< */,
                          uint32_t             segments_len  /**< */,
                          const xcb_segment_t *segments  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_SEGMENT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_segment_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_segment_t segments */
    xcb_parts[4].iov_base = (char *) segments;
    xcb_parts[4].iov_len = segments_len * sizeof(xcb_segment_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_segment
 ** 
 ** @param xcb_connection_t    *c
 ** @param xcb_drawable_t       drawable
 ** @param xcb_gcontext_t       gc
 ** @param uint32_t             segments_len
 ** @param const xcb_segment_t *segments
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_segment (xcb_connection_t    *c  /**< */,
                  xcb_drawable_t       drawable  /**< */,
                  xcb_gcontext_t       gc  /**< */,
                  uint32_t             segments_len  /**< */,
                  const xcb_segment_t *segments  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_SEGMENT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_segment_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_segment_t segments */
    xcb_parts[4].iov_base = (char *) segments;
    xcb_parts[4].iov_len = segments_len * sizeof(xcb_segment_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_poly_rectangle_sizeof (const void  *_buffer  /**< */,
                           uint32_t     rectangles_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_rectangle_request_t);
    xcb_tmp += xcb_block_len;
    /* rectangles */
    xcb_block_len += rectangles_len * sizeof(xcb_rectangle_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_rectangle_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_rectangle_checked
 ** 
 ** @param xcb_connection_t      *c
 ** @param xcb_drawable_t         drawable
 ** @param xcb_gcontext_t         gc
 ** @param uint32_t               rectangles_len
 ** @param const xcb_rectangle_t *rectangles
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_rectangle_checked (xcb_connection_t      *c  /**< */,
                            xcb_drawable_t         drawable  /**< */,
                            xcb_gcontext_t         gc  /**< */,
                            uint32_t               rectangles_len  /**< */,
                            const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_RECTANGLE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_rectangle_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rectangles */
    xcb_parts[4].iov_base = (char *) rectangles;
    xcb_parts[4].iov_len = rectangles_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_rectangle
 ** 
 ** @param xcb_connection_t      *c
 ** @param xcb_drawable_t         drawable
 ** @param xcb_gcontext_t         gc
 ** @param uint32_t               rectangles_len
 ** @param const xcb_rectangle_t *rectangles
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_rectangle (xcb_connection_t      *c  /**< */,
                    xcb_drawable_t         drawable  /**< */,
                    xcb_gcontext_t         gc  /**< */,
                    uint32_t               rectangles_len  /**< */,
                    const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_RECTANGLE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_rectangle_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rectangles */
    xcb_parts[4].iov_base = (char *) rectangles;
    xcb_parts[4].iov_len = rectangles_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_poly_arc_sizeof (const void  *_buffer  /**< */,
                     uint32_t     arcs_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_arc_request_t);
    xcb_tmp += xcb_block_len;
    /* arcs */
    xcb_block_len += arcs_len * sizeof(xcb_arc_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_arc_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_arc_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param uint32_t          arcs_len
 ** @param const xcb_arc_t  *arcs
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_arc_checked (xcb_connection_t *c  /**< */,
                      xcb_drawable_t    drawable  /**< */,
                      xcb_gcontext_t    gc  /**< */,
                      uint32_t          arcs_len  /**< */,
                      const xcb_arc_t  *arcs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_ARC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_arc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_arc_t arcs */
    xcb_parts[4].iov_base = (char *) arcs;
    xcb_parts[4].iov_len = arcs_len * sizeof(xcb_arc_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_arc
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param uint32_t          arcs_len
 ** @param const xcb_arc_t  *arcs
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_arc (xcb_connection_t *c  /**< */,
              xcb_drawable_t    drawable  /**< */,
              xcb_gcontext_t    gc  /**< */,
              uint32_t          arcs_len  /**< */,
              const xcb_arc_t  *arcs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_ARC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_arc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_arc_t arcs */
    xcb_parts[4].iov_base = (char *) arcs;
    xcb_parts[4].iov_len = arcs_len * sizeof(xcb_arc_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_fill_poly_sizeof (const void  *_buffer  /**< */,
                      uint32_t     points_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_fill_poly_request_t);
    xcb_tmp += xcb_block_len;
    /* points */
    xcb_block_len += points_len * sizeof(xcb_point_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_point_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_fill_poly_checked
 ** 
 ** @param xcb_connection_t  *c
 ** @param xcb_drawable_t     drawable
 ** @param xcb_gcontext_t     gc
 ** @param uint8_t            shape
 ** @param uint8_t            coordinate_mode
 ** @param uint32_t           points_len
 ** @param const xcb_point_t *points
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_fill_poly_checked (xcb_connection_t  *c  /**< */,
                       xcb_drawable_t     drawable  /**< */,
                       xcb_gcontext_t     gc  /**< */,
                       uint8_t            shape  /**< */,
                       uint8_t            coordinate_mode  /**< */,
                       uint32_t           points_len  /**< */,
                       const xcb_point_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_FILL_POLY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_fill_poly_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.shape = shape;
    xcb_out.coordinate_mode = coordinate_mode;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_point_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_point_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_fill_poly
 ** 
 ** @param xcb_connection_t  *c
 ** @param xcb_drawable_t     drawable
 ** @param xcb_gcontext_t     gc
 ** @param uint8_t            shape
 ** @param uint8_t            coordinate_mode
 ** @param uint32_t           points_len
 ** @param const xcb_point_t *points
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_fill_poly (xcb_connection_t  *c  /**< */,
               xcb_drawable_t     drawable  /**< */,
               xcb_gcontext_t     gc  /**< */,
               uint8_t            shape  /**< */,
               uint8_t            coordinate_mode  /**< */,
               uint32_t           points_len  /**< */,
               const xcb_point_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_FILL_POLY,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_fill_poly_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.shape = shape;
    xcb_out.coordinate_mode = coordinate_mode;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_point_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_point_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_poly_fill_rectangle_sizeof (const void  *_buffer  /**< */,
                                uint32_t     rectangles_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_fill_rectangle_request_t);
    xcb_tmp += xcb_block_len;
    /* rectangles */
    xcb_block_len += rectangles_len * sizeof(xcb_rectangle_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_rectangle_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_fill_rectangle_checked
 ** 
 ** @param xcb_connection_t      *c
 ** @param xcb_drawable_t         drawable
 ** @param xcb_gcontext_t         gc
 ** @param uint32_t               rectangles_len
 ** @param const xcb_rectangle_t *rectangles
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_fill_rectangle_checked (xcb_connection_t      *c  /**< */,
                                 xcb_drawable_t         drawable  /**< */,
                                 xcb_gcontext_t         gc  /**< */,
                                 uint32_t               rectangles_len  /**< */,
                                 const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_FILL_RECTANGLE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_fill_rectangle_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rectangles */
    xcb_parts[4].iov_base = (char *) rectangles;
    xcb_parts[4].iov_len = rectangles_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_fill_rectangle
 ** 
 ** @param xcb_connection_t      *c
 ** @param xcb_drawable_t         drawable
 ** @param xcb_gcontext_t         gc
 ** @param uint32_t               rectangles_len
 ** @param const xcb_rectangle_t *rectangles
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_fill_rectangle (xcb_connection_t      *c  /**< */,
                         xcb_drawable_t         drawable  /**< */,
                         xcb_gcontext_t         gc  /**< */,
                         uint32_t               rectangles_len  /**< */,
                         const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_FILL_RECTANGLE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_fill_rectangle_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rectangles */
    xcb_parts[4].iov_base = (char *) rectangles;
    xcb_parts[4].iov_len = rectangles_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_poly_fill_arc_sizeof (const void  *_buffer  /**< */,
                          uint32_t     arcs_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_fill_arc_request_t);
    xcb_tmp += xcb_block_len;
    /* arcs */
    xcb_block_len += arcs_len * sizeof(xcb_arc_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_arc_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_fill_arc_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param uint32_t          arcs_len
 ** @param const xcb_arc_t  *arcs
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_fill_arc_checked (xcb_connection_t *c  /**< */,
                           xcb_drawable_t    drawable  /**< */,
                           xcb_gcontext_t    gc  /**< */,
                           uint32_t          arcs_len  /**< */,
                           const xcb_arc_t  *arcs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_FILL_ARC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_fill_arc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_arc_t arcs */
    xcb_parts[4].iov_base = (char *) arcs;
    xcb_parts[4].iov_len = arcs_len * sizeof(xcb_arc_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_fill_arc
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param uint32_t          arcs_len
 ** @param const xcb_arc_t  *arcs
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_fill_arc (xcb_connection_t *c  /**< */,
                   xcb_drawable_t    drawable  /**< */,
                   xcb_gcontext_t    gc  /**< */,
                   uint32_t          arcs_len  /**< */,
                   const xcb_arc_t  *arcs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_FILL_ARC,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_fill_arc_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_arc_t arcs */
    xcb_parts[4].iov_base = (char *) arcs;
    xcb_parts[4].iov_len = arcs_len * sizeof(xcb_arc_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_put_image_sizeof (const void  *_buffer  /**< */,
                      uint32_t     data_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_put_image_request_t);
    xcb_tmp += xcb_block_len;
    /* data */
    xcb_block_len += data_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_put_image_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           format
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @param int16_t           dst_x
 ** @param int16_t           dst_y
 ** @param uint8_t           left_pad
 ** @param uint8_t           depth
 ** @param uint32_t          data_len
 ** @param const uint8_t    *data
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_put_image_checked (xcb_connection_t *c  /**< */,
                       uint8_t           format  /**< */,
                       xcb_drawable_t    drawable  /**< */,
                       xcb_gcontext_t    gc  /**< */,
                       uint16_t          width  /**< */,
                       uint16_t          height  /**< */,
                       int16_t           dst_x  /**< */,
                       int16_t           dst_y  /**< */,
                       uint8_t           left_pad  /**< */,
                       uint8_t           depth  /**< */,
                       uint32_t          data_len  /**< */,
                       const uint8_t    *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_PUT_IMAGE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_put_image_request_t xcb_out;
    
    xcb_out.format = format;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.left_pad = left_pad;
    xcb_out.depth = depth;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_put_image
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           format
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @param int16_t           dst_x
 ** @param int16_t           dst_y
 ** @param uint8_t           left_pad
 ** @param uint8_t           depth
 ** @param uint32_t          data_len
 ** @param const uint8_t    *data
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_put_image (xcb_connection_t *c  /**< */,
               uint8_t           format  /**< */,
               xcb_drawable_t    drawable  /**< */,
               xcb_gcontext_t    gc  /**< */,
               uint16_t          width  /**< */,
               uint16_t          height  /**< */,
               int16_t           dst_x  /**< */,
               int16_t           dst_y  /**< */,
               uint8_t           left_pad  /**< */,
               uint8_t           depth  /**< */,
               uint32_t          data_len  /**< */,
               const uint8_t    *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_PUT_IMAGE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_put_image_request_t xcb_out;
    
    xcb_out.format = format;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.left_pad = left_pad;
    xcb_out.depth = depth;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_get_image_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_get_image_reply_t *_aux = (xcb_get_image_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_get_image_reply_t);
    xcb_tmp += xcb_block_len;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_get_image_cookie_t xcb_get_image
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           format
 ** @param xcb_drawable_t    drawable
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @param uint32_t          plane_mask
 ** @returns xcb_get_image_cookie_t
 **
 *****************************************************************************/
 
xcb_get_image_cookie_t
xcb_get_image (xcb_connection_t *c  /**< */,
               uint8_t           format  /**< */,
               xcb_drawable_t    drawable  /**< */,
               int16_t           x  /**< */,
               int16_t           y  /**< */,
               uint16_t          width  /**< */,
               uint16_t          height  /**< */,
               uint32_t          plane_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_IMAGE,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_image_cookie_t xcb_ret;
    xcb_get_image_request_t xcb_out;
    
    xcb_out.format = format;
    xcb_out.drawable = drawable;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.plane_mask = plane_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_image_cookie_t xcb_get_image_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           format
 ** @param xcb_drawable_t    drawable
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @param uint32_t          plane_mask
 ** @returns xcb_get_image_cookie_t
 **
 *****************************************************************************/
 
xcb_get_image_cookie_t
xcb_get_image_unchecked (xcb_connection_t *c  /**< */,
                         uint8_t           format  /**< */,
                         xcb_drawable_t    drawable  /**< */,
                         int16_t           x  /**< */,
                         int16_t           y  /**< */,
                         uint16_t          width  /**< */,
                         uint16_t          height  /**< */,
                         uint32_t          plane_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_IMAGE,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_image_cookie_t xcb_ret;
    xcb_get_image_request_t xcb_out;
    
    xcb_out.format = format;
    xcb_out.drawable = drawable;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.plane_mask = plane_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** uint8_t * xcb_get_image_data
 ** 
 ** @param const xcb_get_image_reply_t *R
 ** @returns uint8_t *
 **
 *****************************************************************************/
 
uint8_t *
xcb_get_image_data (const xcb_get_image_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_get_image_data_length
 ** 
 ** @param const xcb_get_image_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_get_image_data_length (const xcb_get_image_reply_t *R  /**< */)
{
    return (R->length * 4);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_get_image_data_end
 ** 
 ** @param const xcb_get_image_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_get_image_data_end (const xcb_get_image_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_get_image_reply_t * xcb_get_image_reply
 ** 
 ** @param xcb_connection_t        *c
 ** @param xcb_get_image_cookie_t   cookie
 ** @param xcb_generic_error_t    **e
 ** @returns xcb_get_image_reply_t *
 **
 *****************************************************************************/
 
xcb_get_image_reply_t *
xcb_get_image_reply (xcb_connection_t        *c  /**< */,
                     xcb_get_image_cookie_t   cookie  /**< */,
                     xcb_generic_error_t    **e  /**< */)
{
    return (xcb_get_image_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_poly_text_8_sizeof (const void  *_buffer  /**< */,
                        uint32_t     items_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_text_8_request_t);
    xcb_tmp += xcb_block_len;
    /* items */
    xcb_block_len += items_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_text_8_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint32_t          items_len
 ** @param const uint8_t    *items
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_text_8_checked (xcb_connection_t *c  /**< */,
                         xcb_drawable_t    drawable  /**< */,
                         xcb_gcontext_t    gc  /**< */,
                         int16_t           x  /**< */,
                         int16_t           y  /**< */,
                         uint32_t          items_len  /**< */,
                         const uint8_t    *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_TEXT_8,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_text_8_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len = items_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_text_8
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint32_t          items_len
 ** @param const uint8_t    *items
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_text_8 (xcb_connection_t *c  /**< */,
                 xcb_drawable_t    drawable  /**< */,
                 xcb_gcontext_t    gc  /**< */,
                 int16_t           x  /**< */,
                 int16_t           y  /**< */,
                 uint32_t          items_len  /**< */,
                 const uint8_t    *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_TEXT_8,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_text_8_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len = items_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_poly_text_16_sizeof (const void  *_buffer  /**< */,
                         uint32_t     items_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_poly_text_16_request_t);
    xcb_tmp += xcb_block_len;
    /* items */
    xcb_block_len += items_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_text_16_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint32_t          items_len
 ** @param const uint8_t    *items
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_text_16_checked (xcb_connection_t *c  /**< */,
                          xcb_drawable_t    drawable  /**< */,
                          xcb_gcontext_t    gc  /**< */,
                          int16_t           x  /**< */,
                          int16_t           y  /**< */,
                          uint32_t          items_len  /**< */,
                          const uint8_t    *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_TEXT_16,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_text_16_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len = items_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_poly_text_16
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param uint32_t          items_len
 ** @param const uint8_t    *items
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_poly_text_16 (xcb_connection_t *c  /**< */,
                  xcb_drawable_t    drawable  /**< */,
                  xcb_gcontext_t    gc  /**< */,
                  int16_t           x  /**< */,
                  int16_t           y  /**< */,
                  uint32_t          items_len  /**< */,
                  const uint8_t    *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_POLY_TEXT_16,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_text_16_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len = items_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_image_text_8_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_image_text_8_request_t *_aux = (xcb_image_text_8_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_image_text_8_request_t);
    xcb_tmp += xcb_block_len;
    /* string */
    xcb_block_len += _aux->string_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_image_text_8_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           string_len
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param const char       *string
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_image_text_8_checked (xcb_connection_t *c  /**< */,
                          uint8_t           string_len  /**< */,
                          xcb_drawable_t    drawable  /**< */,
                          xcb_gcontext_t    gc  /**< */,
                          int16_t           x  /**< */,
                          int16_t           y  /**< */,
                          const char       *string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_IMAGE_TEXT_8,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_image_text_8_request_t xcb_out;
    
    xcb_out.string_len = string_len;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char string */
    xcb_parts[4].iov_base = (char *) string;
    xcb_parts[4].iov_len = string_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_image_text_8
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           string_len
 ** @param xcb_drawable_t    drawable
 ** @param xcb_gcontext_t    gc
 ** @param int16_t           x
 ** @param int16_t           y
 ** @param const char       *string
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_image_text_8 (xcb_connection_t *c  /**< */,
                  uint8_t           string_len  /**< */,
                  xcb_drawable_t    drawable  /**< */,
                  xcb_gcontext_t    gc  /**< */,
                  int16_t           x  /**< */,
                  int16_t           y  /**< */,
                  const char       *string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_IMAGE_TEXT_8,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_image_text_8_request_t xcb_out;
    
    xcb_out.string_len = string_len;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char string */
    xcb_parts[4].iov_base = (char *) string;
    xcb_parts[4].iov_len = string_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_image_text_16_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_image_text_16_request_t *_aux = (xcb_image_text_16_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_image_text_16_request_t);
    xcb_tmp += xcb_block_len;
    /* string */
    xcb_block_len += _aux->string_len * sizeof(xcb_char2b_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_char2b_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_image_text_16_checked
 ** 
 ** @param xcb_connection_t   *c
 ** @param uint8_t             string_len
 ** @param xcb_drawable_t      drawable
 ** @param xcb_gcontext_t      gc
 ** @param int16_t             x
 ** @param int16_t             y
 ** @param const xcb_char2b_t *string
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_image_text_16_checked (xcb_connection_t   *c  /**< */,
                           uint8_t             string_len  /**< */,
                           xcb_drawable_t      drawable  /**< */,
                           xcb_gcontext_t      gc  /**< */,
                           int16_t             x  /**< */,
                           int16_t             y  /**< */,
                           const xcb_char2b_t *string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_IMAGE_TEXT_16,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_image_text_16_request_t xcb_out;
    
    xcb_out.string_len = string_len;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_char2b_t string */
    xcb_parts[4].iov_base = (char *) string;
    xcb_parts[4].iov_len = string_len * sizeof(xcb_char2b_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_image_text_16
 ** 
 ** @param xcb_connection_t   *c
 ** @param uint8_t             string_len
 ** @param xcb_drawable_t      drawable
 ** @param xcb_gcontext_t      gc
 ** @param int16_t             x
 ** @param int16_t             y
 ** @param const xcb_char2b_t *string
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_image_text_16 (xcb_connection_t   *c  /**< */,
                   uint8_t             string_len  /**< */,
                   xcb_drawable_t      drawable  /**< */,
                   xcb_gcontext_t      gc  /**< */,
                   int16_t             x  /**< */,
                   int16_t             y  /**< */,
                   const xcb_char2b_t *string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_IMAGE_TEXT_16,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_image_text_16_request_t xcb_out;
    
    xcb_out.string_len = string_len;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_char2b_t string */
    xcb_parts[4].iov_base = (char *) string;
    xcb_parts[4].iov_len = string_len * sizeof(xcb_char2b_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_colormap_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           alloc
 ** @param xcb_colormap_t    mid
 ** @param xcb_window_t      window
 ** @param xcb_visualid_t    visual
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_colormap_checked (xcb_connection_t *c  /**< */,
                             uint8_t           alloc  /**< */,
                             xcb_colormap_t    mid  /**< */,
                             xcb_window_t      window  /**< */,
                             xcb_visualid_t    visual  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_COLORMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_create_colormap_request_t xcb_out;
    
    xcb_out.alloc = alloc;
    xcb_out.mid = mid;
    xcb_out.window = window;
    xcb_out.visual = visual;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_colormap
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           alloc
 ** @param xcb_colormap_t    mid
 ** @param xcb_window_t      window
 ** @param xcb_visualid_t    visual
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_colormap (xcb_connection_t *c  /**< */,
                     uint8_t           alloc  /**< */,
                     xcb_colormap_t    mid  /**< */,
                     xcb_window_t      window  /**< */,
                     xcb_visualid_t    visual  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_COLORMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_create_colormap_request_t xcb_out;
    
    xcb_out.alloc = alloc;
    xcb_out.mid = mid;
    xcb_out.window = window;
    xcb_out.visual = visual;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_colormap_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_colormap_checked (xcb_connection_t *c  /**< */,
                           xcb_colormap_t    cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FREE_COLORMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_free_colormap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_colormap
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_colormap (xcb_connection_t *c  /**< */,
                   xcb_colormap_t    cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FREE_COLORMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_free_colormap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_copy_colormap_and_free_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    mid
 ** @param xcb_colormap_t    src_cmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_copy_colormap_and_free_checked (xcb_connection_t *c  /**< */,
                                    xcb_colormap_t    mid  /**< */,
                                    xcb_colormap_t    src_cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_COPY_COLORMAP_AND_FREE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_copy_colormap_and_free_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.mid = mid;
    xcb_out.src_cmap = src_cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_copy_colormap_and_free
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    mid
 ** @param xcb_colormap_t    src_cmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_copy_colormap_and_free (xcb_connection_t *c  /**< */,
                            xcb_colormap_t    mid  /**< */,
                            xcb_colormap_t    src_cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_COPY_COLORMAP_AND_FREE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_copy_colormap_and_free_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.mid = mid;
    xcb_out.src_cmap = src_cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_install_colormap_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_install_colormap_checked (xcb_connection_t *c  /**< */,
                              xcb_colormap_t    cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_INSTALL_COLORMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_install_colormap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_install_colormap
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_install_colormap (xcb_connection_t *c  /**< */,
                      xcb_colormap_t    cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_INSTALL_COLORMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_install_colormap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_uninstall_colormap_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_uninstall_colormap_checked (xcb_connection_t *c  /**< */,
                                xcb_colormap_t    cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNINSTALL_COLORMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_uninstall_colormap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_uninstall_colormap
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_uninstall_colormap (xcb_connection_t *c  /**< */,
                        xcb_colormap_t    cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_UNINSTALL_COLORMAP,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_uninstall_colormap_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_list_installed_colormaps_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_list_installed_colormaps_reply_t *_aux = (xcb_list_installed_colormaps_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_list_installed_colormaps_reply_t);
    xcb_tmp += xcb_block_len;
    /* cmaps */
    xcb_block_len += _aux->cmaps_len * sizeof(xcb_colormap_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_colormap_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_list_installed_colormaps_cookie_t xcb_list_installed_colormaps
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_list_installed_colormaps_cookie_t
 **
 *****************************************************************************/
 
xcb_list_installed_colormaps_cookie_t
xcb_list_installed_colormaps (xcb_connection_t *c  /**< */,
                              xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_LIST_INSTALLED_COLORMAPS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_list_installed_colormaps_cookie_t xcb_ret;
    xcb_list_installed_colormaps_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_list_installed_colormaps_cookie_t xcb_list_installed_colormaps_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_list_installed_colormaps_cookie_t
 **
 *****************************************************************************/
 
xcb_list_installed_colormaps_cookie_t
xcb_list_installed_colormaps_unchecked (xcb_connection_t *c  /**< */,
                                        xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_LIST_INSTALLED_COLORMAPS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_list_installed_colormaps_cookie_t xcb_ret;
    xcb_list_installed_colormaps_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_colormap_t * xcb_list_installed_colormaps_cmaps
 ** 
 ** @param const xcb_list_installed_colormaps_reply_t *R
 ** @returns xcb_colormap_t *
 **
 *****************************************************************************/
 
xcb_colormap_t *
xcb_list_installed_colormaps_cmaps (const xcb_list_installed_colormaps_reply_t *R  /**< */)
{
    return (xcb_colormap_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_list_installed_colormaps_cmaps_length
 ** 
 ** @param const xcb_list_installed_colormaps_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_list_installed_colormaps_cmaps_length (const xcb_list_installed_colormaps_reply_t *R  /**< */)
{
    return R->cmaps_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_list_installed_colormaps_cmaps_end
 ** 
 ** @param const xcb_list_installed_colormaps_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_list_installed_colormaps_cmaps_end (const xcb_list_installed_colormaps_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_colormap_t *) (R + 1)) + (R->cmaps_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_list_installed_colormaps_reply_t * xcb_list_installed_colormaps_reply
 ** 
 ** @param xcb_connection_t                       *c
 ** @param xcb_list_installed_colormaps_cookie_t   cookie
 ** @param xcb_generic_error_t                   **e
 ** @returns xcb_list_installed_colormaps_reply_t *
 **
 *****************************************************************************/
 
xcb_list_installed_colormaps_reply_t *
xcb_list_installed_colormaps_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_list_installed_colormaps_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_list_installed_colormaps_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_alloc_color_cookie_t xcb_alloc_color
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          red
 ** @param uint16_t          green
 ** @param uint16_t          blue
 ** @returns xcb_alloc_color_cookie_t
 **
 *****************************************************************************/
 
xcb_alloc_color_cookie_t
xcb_alloc_color (xcb_connection_t *c  /**< */,
                 xcb_colormap_t    cmap  /**< */,
                 uint16_t          red  /**< */,
                 uint16_t          green  /**< */,
                 uint16_t          blue  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_ALLOC_COLOR,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_alloc_color_cookie_t xcb_ret;
    xcb_alloc_color_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    xcb_out.red = red;
    xcb_out.green = green;
    xcb_out.blue = blue;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_alloc_color_cookie_t xcb_alloc_color_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          red
 ** @param uint16_t          green
 ** @param uint16_t          blue
 ** @returns xcb_alloc_color_cookie_t
 **
 *****************************************************************************/
 
xcb_alloc_color_cookie_t
xcb_alloc_color_unchecked (xcb_connection_t *c  /**< */,
                           xcb_colormap_t    cmap  /**< */,
                           uint16_t          red  /**< */,
                           uint16_t          green  /**< */,
                           uint16_t          blue  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_ALLOC_COLOR,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_alloc_color_cookie_t xcb_ret;
    xcb_alloc_color_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    xcb_out.red = red;
    xcb_out.green = green;
    xcb_out.blue = blue;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_alloc_color_reply_t * xcb_alloc_color_reply
 ** 
 ** @param xcb_connection_t          *c
 ** @param xcb_alloc_color_cookie_t   cookie
 ** @param xcb_generic_error_t      **e
 ** @returns xcb_alloc_color_reply_t *
 **
 *****************************************************************************/
 
xcb_alloc_color_reply_t *
xcb_alloc_color_reply (xcb_connection_t          *c  /**< */,
                       xcb_alloc_color_cookie_t   cookie  /**< */,
                       xcb_generic_error_t      **e  /**< */)
{
    return (xcb_alloc_color_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_alloc_named_color_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_alloc_named_color_request_t *_aux = (xcb_alloc_named_color_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_alloc_named_color_request_t);
    xcb_tmp += xcb_block_len;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_alloc_named_color_cookie_t xcb_alloc_named_color
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_alloc_named_color_cookie_t
 **
 *****************************************************************************/
 
xcb_alloc_named_color_cookie_t
xcb_alloc_named_color (xcb_connection_t *c  /**< */,
                       xcb_colormap_t    cmap  /**< */,
                       uint16_t          name_len  /**< */,
                       const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_ALLOC_NAMED_COLOR,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_alloc_named_color_cookie_t xcb_ret;
    xcb_alloc_named_color_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_alloc_named_color_cookie_t xcb_alloc_named_color_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_alloc_named_color_cookie_t
 **
 *****************************************************************************/
 
xcb_alloc_named_color_cookie_t
xcb_alloc_named_color_unchecked (xcb_connection_t *c  /**< */,
                                 xcb_colormap_t    cmap  /**< */,
                                 uint16_t          name_len  /**< */,
                                 const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_ALLOC_NAMED_COLOR,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_alloc_named_color_cookie_t xcb_ret;
    xcb_alloc_named_color_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_alloc_named_color_reply_t * xcb_alloc_named_color_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_alloc_named_color_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_alloc_named_color_reply_t *
 **
 *****************************************************************************/
 
xcb_alloc_named_color_reply_t *
xcb_alloc_named_color_reply (xcb_connection_t                *c  /**< */,
                             xcb_alloc_named_color_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_alloc_named_color_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_alloc_color_cells_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_alloc_color_cells_reply_t *_aux = (xcb_alloc_color_cells_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_alloc_color_cells_reply_t);
    xcb_tmp += xcb_block_len;
    /* pixels */
    xcb_block_len += _aux->pixels_len * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* masks */
    xcb_block_len += _aux->masks_len * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_alloc_color_cells_cookie_t xcb_alloc_color_cells
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           contiguous
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          colors
 ** @param uint16_t          planes
 ** @returns xcb_alloc_color_cells_cookie_t
 **
 *****************************************************************************/
 
xcb_alloc_color_cells_cookie_t
xcb_alloc_color_cells (xcb_connection_t *c  /**< */,
                       uint8_t           contiguous  /**< */,
                       xcb_colormap_t    cmap  /**< */,
                       uint16_t          colors  /**< */,
                       uint16_t          planes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_ALLOC_COLOR_CELLS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_alloc_color_cells_cookie_t xcb_ret;
    xcb_alloc_color_cells_request_t xcb_out;
    
    xcb_out.contiguous = contiguous;
    xcb_out.cmap = cmap;
    xcb_out.colors = colors;
    xcb_out.planes = planes;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_alloc_color_cells_cookie_t xcb_alloc_color_cells_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           contiguous
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          colors
 ** @param uint16_t          planes
 ** @returns xcb_alloc_color_cells_cookie_t
 **
 *****************************************************************************/
 
xcb_alloc_color_cells_cookie_t
xcb_alloc_color_cells_unchecked (xcb_connection_t *c  /**< */,
                                 uint8_t           contiguous  /**< */,
                                 xcb_colormap_t    cmap  /**< */,
                                 uint16_t          colors  /**< */,
                                 uint16_t          planes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_ALLOC_COLOR_CELLS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_alloc_color_cells_cookie_t xcb_ret;
    xcb_alloc_color_cells_request_t xcb_out;
    
    xcb_out.contiguous = contiguous;
    xcb_out.cmap = cmap;
    xcb_out.colors = colors;
    xcb_out.planes = planes;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** uint32_t * xcb_alloc_color_cells_pixels
 ** 
 ** @param const xcb_alloc_color_cells_reply_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_alloc_color_cells_pixels (const xcb_alloc_color_cells_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_alloc_color_cells_pixels_length
 ** 
 ** @param const xcb_alloc_color_cells_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_alloc_color_cells_pixels_length (const xcb_alloc_color_cells_reply_t *R  /**< */)
{
    return R->pixels_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_alloc_color_cells_pixels_end
 ** 
 ** @param const xcb_alloc_color_cells_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_alloc_color_cells_pixels_end (const xcb_alloc_color_cells_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->pixels_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** uint32_t * xcb_alloc_color_cells_masks
 ** 
 ** @param const xcb_alloc_color_cells_reply_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_alloc_color_cells_masks (const xcb_alloc_color_cells_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_alloc_color_cells_pixels_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}


/*****************************************************************************
 **
 ** int xcb_alloc_color_cells_masks_length
 ** 
 ** @param const xcb_alloc_color_cells_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_alloc_color_cells_masks_length (const xcb_alloc_color_cells_reply_t *R  /**< */)
{
    return R->masks_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_alloc_color_cells_masks_end
 ** 
 ** @param const xcb_alloc_color_cells_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_alloc_color_cells_masks_end (const xcb_alloc_color_cells_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_alloc_color_cells_pixels_end(R);
    i.data = ((uint32_t *) child.data) + (R->masks_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_alloc_color_cells_reply_t * xcb_alloc_color_cells_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_alloc_color_cells_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_alloc_color_cells_reply_t *
 **
 *****************************************************************************/
 
xcb_alloc_color_cells_reply_t *
xcb_alloc_color_cells_reply (xcb_connection_t                *c  /**< */,
                             xcb_alloc_color_cells_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_alloc_color_cells_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_alloc_color_planes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_alloc_color_planes_reply_t *_aux = (xcb_alloc_color_planes_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_alloc_color_planes_reply_t);
    xcb_tmp += xcb_block_len;
    /* pixels */
    xcb_block_len += _aux->pixels_len * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_alloc_color_planes_cookie_t xcb_alloc_color_planes
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           contiguous
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          colors
 ** @param uint16_t          reds
 ** @param uint16_t          greens
 ** @param uint16_t          blues
 ** @returns xcb_alloc_color_planes_cookie_t
 **
 *****************************************************************************/
 
xcb_alloc_color_planes_cookie_t
xcb_alloc_color_planes (xcb_connection_t *c  /**< */,
                        uint8_t           contiguous  /**< */,
                        xcb_colormap_t    cmap  /**< */,
                        uint16_t          colors  /**< */,
                        uint16_t          reds  /**< */,
                        uint16_t          greens  /**< */,
                        uint16_t          blues  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_ALLOC_COLOR_PLANES,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_alloc_color_planes_cookie_t xcb_ret;
    xcb_alloc_color_planes_request_t xcb_out;
    
    xcb_out.contiguous = contiguous;
    xcb_out.cmap = cmap;
    xcb_out.colors = colors;
    xcb_out.reds = reds;
    xcb_out.greens = greens;
    xcb_out.blues = blues;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_alloc_color_planes_cookie_t xcb_alloc_color_planes_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           contiguous
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          colors
 ** @param uint16_t          reds
 ** @param uint16_t          greens
 ** @param uint16_t          blues
 ** @returns xcb_alloc_color_planes_cookie_t
 **
 *****************************************************************************/
 
xcb_alloc_color_planes_cookie_t
xcb_alloc_color_planes_unchecked (xcb_connection_t *c  /**< */,
                                  uint8_t           contiguous  /**< */,
                                  xcb_colormap_t    cmap  /**< */,
                                  uint16_t          colors  /**< */,
                                  uint16_t          reds  /**< */,
                                  uint16_t          greens  /**< */,
                                  uint16_t          blues  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_ALLOC_COLOR_PLANES,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_alloc_color_planes_cookie_t xcb_ret;
    xcb_alloc_color_planes_request_t xcb_out;
    
    xcb_out.contiguous = contiguous;
    xcb_out.cmap = cmap;
    xcb_out.colors = colors;
    xcb_out.reds = reds;
    xcb_out.greens = greens;
    xcb_out.blues = blues;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** uint32_t * xcb_alloc_color_planes_pixels
 ** 
 ** @param const xcb_alloc_color_planes_reply_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_alloc_color_planes_pixels (const xcb_alloc_color_planes_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_alloc_color_planes_pixels_length
 ** 
 ** @param const xcb_alloc_color_planes_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_alloc_color_planes_pixels_length (const xcb_alloc_color_planes_reply_t *R  /**< */)
{
    return R->pixels_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_alloc_color_planes_pixels_end
 ** 
 ** @param const xcb_alloc_color_planes_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_alloc_color_planes_pixels_end (const xcb_alloc_color_planes_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->pixels_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_alloc_color_planes_reply_t * xcb_alloc_color_planes_reply
 ** 
 ** @param xcb_connection_t                 *c
 ** @param xcb_alloc_color_planes_cookie_t   cookie
 ** @param xcb_generic_error_t             **e
 ** @returns xcb_alloc_color_planes_reply_t *
 **
 *****************************************************************************/
 
xcb_alloc_color_planes_reply_t *
xcb_alloc_color_planes_reply (xcb_connection_t                 *c  /**< */,
                              xcb_alloc_color_planes_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_alloc_color_planes_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_free_colors_sizeof (const void  *_buffer  /**< */,
                        uint32_t     pixels_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_free_colors_request_t);
    xcb_tmp += xcb_block_len;
    /* pixels */
    xcb_block_len += pixels_len * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_colors_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint32_t          plane_mask
 ** @param uint32_t          pixels_len
 ** @param const uint32_t   *pixels
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_colors_checked (xcb_connection_t *c  /**< */,
                         xcb_colormap_t    cmap  /**< */,
                         uint32_t          plane_mask  /**< */,
                         uint32_t          pixels_len  /**< */,
                         const uint32_t   *pixels  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_FREE_COLORS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_free_colors_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    xcb_out.plane_mask = plane_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t pixels */
    xcb_parts[4].iov_base = (char *) pixels;
    xcb_parts[4].iov_len = pixels_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_colors
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint32_t          plane_mask
 ** @param uint32_t          pixels_len
 ** @param const uint32_t   *pixels
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_colors (xcb_connection_t *c  /**< */,
                 xcb_colormap_t    cmap  /**< */,
                 uint32_t          plane_mask  /**< */,
                 uint32_t          pixels_len  /**< */,
                 const uint32_t   *pixels  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_FREE_COLORS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_free_colors_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    xcb_out.plane_mask = plane_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t pixels */
    xcb_parts[4].iov_base = (char *) pixels;
    xcb_parts[4].iov_len = pixels_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** void xcb_coloritem_next
 ** 
 ** @param xcb_coloritem_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_coloritem_next (xcb_coloritem_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_coloritem_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_coloritem_end
 ** 
 ** @param xcb_coloritem_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_coloritem_end (xcb_coloritem_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_store_colors_sizeof (const void  *_buffer  /**< */,
                         uint32_t     items_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_store_colors_request_t);
    xcb_tmp += xcb_block_len;
    /* items */
    xcb_block_len += items_len * sizeof(xcb_coloritem_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_coloritem_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_store_colors_checked
 ** 
 ** @param xcb_connection_t      *c
 ** @param xcb_colormap_t         cmap
 ** @param uint32_t               items_len
 ** @param const xcb_coloritem_t *items
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_store_colors_checked (xcb_connection_t      *c  /**< */,
                          xcb_colormap_t         cmap  /**< */,
                          uint32_t               items_len  /**< */,
                          const xcb_coloritem_t *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_STORE_COLORS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_store_colors_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_coloritem_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len = items_len * sizeof(xcb_coloritem_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_store_colors
 ** 
 ** @param xcb_connection_t      *c
 ** @param xcb_colormap_t         cmap
 ** @param uint32_t               items_len
 ** @param const xcb_coloritem_t *items
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_store_colors (xcb_connection_t      *c  /**< */,
                  xcb_colormap_t         cmap  /**< */,
                  uint32_t               items_len  /**< */,
                  const xcb_coloritem_t *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_STORE_COLORS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_store_colors_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_coloritem_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len = items_len * sizeof(xcb_coloritem_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_store_named_color_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_store_named_color_request_t *_aux = (xcb_store_named_color_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_store_named_color_request_t);
    xcb_tmp += xcb_block_len;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_store_named_color_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           flags
 ** @param xcb_colormap_t    cmap
 ** @param uint32_t          pixel
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_store_named_color_checked (xcb_connection_t *c  /**< */,
                               uint8_t           flags  /**< */,
                               xcb_colormap_t    cmap  /**< */,
                               uint32_t          pixel  /**< */,
                               uint16_t          name_len  /**< */,
                               const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_STORE_NAMED_COLOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_store_named_color_request_t xcb_out;
    
    xcb_out.flags = flags;
    xcb_out.cmap = cmap;
    xcb_out.pixel = pixel;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_store_named_color
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           flags
 ** @param xcb_colormap_t    cmap
 ** @param uint32_t          pixel
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_store_named_color (xcb_connection_t *c  /**< */,
                       uint8_t           flags  /**< */,
                       xcb_colormap_t    cmap  /**< */,
                       uint32_t          pixel  /**< */,
                       uint16_t          name_len  /**< */,
                       const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_STORE_NAMED_COLOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_store_named_color_request_t xcb_out;
    
    xcb_out.flags = flags;
    xcb_out.cmap = cmap;
    xcb_out.pixel = pixel;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** void xcb_rgb_next
 ** 
 ** @param xcb_rgb_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_rgb_next (xcb_rgb_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_rgb_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_rgb_end
 ** 
 ** @param xcb_rgb_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_rgb_end (xcb_rgb_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_query_colors_sizeof (const void  *_buffer  /**< */,
                         uint32_t     pixels_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_query_colors_request_t);
    xcb_tmp += xcb_block_len;
    /* pixels */
    xcb_block_len += pixels_len * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_query_colors_cookie_t xcb_query_colors
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint32_t          pixels_len
 ** @param const uint32_t   *pixels
 ** @returns xcb_query_colors_cookie_t
 **
 *****************************************************************************/
 
xcb_query_colors_cookie_t
xcb_query_colors (xcb_connection_t *c  /**< */,
                  xcb_colormap_t    cmap  /**< */,
                  uint32_t          pixels_len  /**< */,
                  const uint32_t   *pixels  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_COLORS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_query_colors_cookie_t xcb_ret;
    xcb_query_colors_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t pixels */
    xcb_parts[4].iov_base = (char *) pixels;
    xcb_parts[4].iov_len = pixels_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_colors_cookie_t xcb_query_colors_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint32_t          pixels_len
 ** @param const uint32_t   *pixels
 ** @returns xcb_query_colors_cookie_t
 **
 *****************************************************************************/
 
xcb_query_colors_cookie_t
xcb_query_colors_unchecked (xcb_connection_t *c  /**< */,
                            xcb_colormap_t    cmap  /**< */,
                            uint32_t          pixels_len  /**< */,
                            const uint32_t   *pixels  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_COLORS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_query_colors_cookie_t xcb_ret;
    xcb_query_colors_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t pixels */
    xcb_parts[4].iov_base = (char *) pixels;
    xcb_parts[4].iov_len = pixels_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_rgb_t * xcb_query_colors_colors
 ** 
 ** @param const xcb_query_colors_reply_t *R
 ** @returns xcb_rgb_t *
 **
 *****************************************************************************/
 
xcb_rgb_t *
xcb_query_colors_colors (const xcb_query_colors_reply_t *R  /**< */)
{
    return (xcb_rgb_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_query_colors_colors_length
 ** 
 ** @param const xcb_query_colors_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_query_colors_colors_length (const xcb_query_colors_reply_t *R  /**< */)
{
    return R->colors_len;
}


/*****************************************************************************
 **
 ** xcb_rgb_iterator_t xcb_query_colors_colors_iterator
 ** 
 ** @param const xcb_query_colors_reply_t *R
 ** @returns xcb_rgb_iterator_t
 **
 *****************************************************************************/
 
xcb_rgb_iterator_t
xcb_query_colors_colors_iterator (const xcb_query_colors_reply_t *R  /**< */)
{
    xcb_rgb_iterator_t i;
    i.data = (xcb_rgb_t *) (R + 1);
    i.rem = R->colors_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_query_colors_reply_t * xcb_query_colors_reply
 ** 
 ** @param xcb_connection_t           *c
 ** @param xcb_query_colors_cookie_t   cookie
 ** @param xcb_generic_error_t       **e
 ** @returns xcb_query_colors_reply_t *
 **
 *****************************************************************************/
 
xcb_query_colors_reply_t *
xcb_query_colors_reply (xcb_connection_t           *c  /**< */,
                        xcb_query_colors_cookie_t   cookie  /**< */,
                        xcb_generic_error_t       **e  /**< */)
{
    return (xcb_query_colors_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_lookup_color_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_lookup_color_request_t *_aux = (xcb_lookup_color_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_lookup_color_request_t);
    xcb_tmp += xcb_block_len;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_lookup_color_cookie_t xcb_lookup_color
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_lookup_color_cookie_t
 **
 *****************************************************************************/
 
xcb_lookup_color_cookie_t
xcb_lookup_color (xcb_connection_t *c  /**< */,
                  xcb_colormap_t    cmap  /**< */,
                  uint16_t          name_len  /**< */,
                  const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_LOOKUP_COLOR,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_lookup_color_cookie_t xcb_ret;
    xcb_lookup_color_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_lookup_color_cookie_t xcb_lookup_color_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_colormap_t    cmap
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_lookup_color_cookie_t
 **
 *****************************************************************************/
 
xcb_lookup_color_cookie_t
xcb_lookup_color_unchecked (xcb_connection_t *c  /**< */,
                            xcb_colormap_t    cmap  /**< */,
                            uint16_t          name_len  /**< */,
                            const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_LOOKUP_COLOR,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_lookup_color_cookie_t xcb_ret;
    xcb_lookup_color_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cmap = cmap;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_lookup_color_reply_t * xcb_lookup_color_reply
 ** 
 ** @param xcb_connection_t           *c
 ** @param xcb_lookup_color_cookie_t   cookie
 ** @param xcb_generic_error_t       **e
 ** @returns xcb_lookup_color_reply_t *
 **
 *****************************************************************************/
 
xcb_lookup_color_reply_t *
xcb_lookup_color_reply (xcb_connection_t           *c  /**< */,
                        xcb_lookup_color_cookie_t   cookie  /**< */,
                        xcb_generic_error_t       **e  /**< */)
{
    return (xcb_lookup_color_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_cursor_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cid
 ** @param xcb_pixmap_t      source
 ** @param xcb_pixmap_t      mask
 ** @param uint16_t          fore_red
 ** @param uint16_t          fore_green
 ** @param uint16_t          fore_blue
 ** @param uint16_t          back_red
 ** @param uint16_t          back_green
 ** @param uint16_t          back_blue
 ** @param uint16_t          x
 ** @param uint16_t          y
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_cursor_checked (xcb_connection_t *c  /**< */,
                           xcb_cursor_t      cid  /**< */,
                           xcb_pixmap_t      source  /**< */,
                           xcb_pixmap_t      mask  /**< */,
                           uint16_t          fore_red  /**< */,
                           uint16_t          fore_green  /**< */,
                           uint16_t          fore_blue  /**< */,
                           uint16_t          back_red  /**< */,
                           uint16_t          back_green  /**< */,
                           uint16_t          back_blue  /**< */,
                           uint16_t          x  /**< */,
                           uint16_t          y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_CURSOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_create_cursor_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cid = cid;
    xcb_out.source = source;
    xcb_out.mask = mask;
    xcb_out.fore_red = fore_red;
    xcb_out.fore_green = fore_green;
    xcb_out.fore_blue = fore_blue;
    xcb_out.back_red = back_red;
    xcb_out.back_green = back_green;
    xcb_out.back_blue = back_blue;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_cursor
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cid
 ** @param xcb_pixmap_t      source
 ** @param xcb_pixmap_t      mask
 ** @param uint16_t          fore_red
 ** @param uint16_t          fore_green
 ** @param uint16_t          fore_blue
 ** @param uint16_t          back_red
 ** @param uint16_t          back_green
 ** @param uint16_t          back_blue
 ** @param uint16_t          x
 ** @param uint16_t          y
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_cursor (xcb_connection_t *c  /**< */,
                   xcb_cursor_t      cid  /**< */,
                   xcb_pixmap_t      source  /**< */,
                   xcb_pixmap_t      mask  /**< */,
                   uint16_t          fore_red  /**< */,
                   uint16_t          fore_green  /**< */,
                   uint16_t          fore_blue  /**< */,
                   uint16_t          back_red  /**< */,
                   uint16_t          back_green  /**< */,
                   uint16_t          back_blue  /**< */,
                   uint16_t          x  /**< */,
                   uint16_t          y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_CURSOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_create_cursor_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cid = cid;
    xcb_out.source = source;
    xcb_out.mask = mask;
    xcb_out.fore_red = fore_red;
    xcb_out.fore_green = fore_green;
    xcb_out.fore_blue = fore_blue;
    xcb_out.back_red = back_red;
    xcb_out.back_green = back_green;
    xcb_out.back_blue = back_blue;
    xcb_out.x = x;
    xcb_out.y = y;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_glyph_cursor_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cid
 ** @param xcb_font_t        source_font
 ** @param xcb_font_t        mask_font
 ** @param uint16_t          source_char
 ** @param uint16_t          mask_char
 ** @param uint16_t          fore_red
 ** @param uint16_t          fore_green
 ** @param uint16_t          fore_blue
 ** @param uint16_t          back_red
 ** @param uint16_t          back_green
 ** @param uint16_t          back_blue
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_glyph_cursor_checked (xcb_connection_t *c  /**< */,
                                 xcb_cursor_t      cid  /**< */,
                                 xcb_font_t        source_font  /**< */,
                                 xcb_font_t        mask_font  /**< */,
                                 uint16_t          source_char  /**< */,
                                 uint16_t          mask_char  /**< */,
                                 uint16_t          fore_red  /**< */,
                                 uint16_t          fore_green  /**< */,
                                 uint16_t          fore_blue  /**< */,
                                 uint16_t          back_red  /**< */,
                                 uint16_t          back_green  /**< */,
                                 uint16_t          back_blue  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_GLYPH_CURSOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_create_glyph_cursor_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cid = cid;
    xcb_out.source_font = source_font;
    xcb_out.mask_font = mask_font;
    xcb_out.source_char = source_char;
    xcb_out.mask_char = mask_char;
    xcb_out.fore_red = fore_red;
    xcb_out.fore_green = fore_green;
    xcb_out.fore_blue = fore_blue;
    xcb_out.back_red = back_red;
    xcb_out.back_green = back_green;
    xcb_out.back_blue = back_blue;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_create_glyph_cursor
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cid
 ** @param xcb_font_t        source_font
 ** @param xcb_font_t        mask_font
 ** @param uint16_t          source_char
 ** @param uint16_t          mask_char
 ** @param uint16_t          fore_red
 ** @param uint16_t          fore_green
 ** @param uint16_t          fore_blue
 ** @param uint16_t          back_red
 ** @param uint16_t          back_green
 ** @param uint16_t          back_blue
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_create_glyph_cursor (xcb_connection_t *c  /**< */,
                         xcb_cursor_t      cid  /**< */,
                         xcb_font_t        source_font  /**< */,
                         xcb_font_t        mask_font  /**< */,
                         uint16_t          source_char  /**< */,
                         uint16_t          mask_char  /**< */,
                         uint16_t          fore_red  /**< */,
                         uint16_t          fore_green  /**< */,
                         uint16_t          fore_blue  /**< */,
                         uint16_t          back_red  /**< */,
                         uint16_t          back_green  /**< */,
                         uint16_t          back_blue  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CREATE_GLYPH_CURSOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_create_glyph_cursor_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cid = cid;
    xcb_out.source_font = source_font;
    xcb_out.mask_font = mask_font;
    xcb_out.source_char = source_char;
    xcb_out.mask_char = mask_char;
    xcb_out.fore_red = fore_red;
    xcb_out.fore_green = fore_green;
    xcb_out.fore_blue = fore_blue;
    xcb_out.back_red = back_red;
    xcb_out.back_green = back_green;
    xcb_out.back_blue = back_blue;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_cursor_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cursor
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_cursor_checked (xcb_connection_t *c  /**< */,
                         xcb_cursor_t      cursor  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FREE_CURSOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_free_cursor_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cursor = cursor;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_free_cursor
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cursor
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_free_cursor (xcb_connection_t *c  /**< */,
                 xcb_cursor_t      cursor  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FREE_CURSOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_free_cursor_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cursor = cursor;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_recolor_cursor_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cursor
 ** @param uint16_t          fore_red
 ** @param uint16_t          fore_green
 ** @param uint16_t          fore_blue
 ** @param uint16_t          back_red
 ** @param uint16_t          back_green
 ** @param uint16_t          back_blue
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_recolor_cursor_checked (xcb_connection_t *c  /**< */,
                            xcb_cursor_t      cursor  /**< */,
                            uint16_t          fore_red  /**< */,
                            uint16_t          fore_green  /**< */,
                            uint16_t          fore_blue  /**< */,
                            uint16_t          back_red  /**< */,
                            uint16_t          back_green  /**< */,
                            uint16_t          back_blue  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_RECOLOR_CURSOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_recolor_cursor_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cursor = cursor;
    xcb_out.fore_red = fore_red;
    xcb_out.fore_green = fore_green;
    xcb_out.fore_blue = fore_blue;
    xcb_out.back_red = back_red;
    xcb_out.back_green = back_green;
    xcb_out.back_blue = back_blue;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_recolor_cursor
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_cursor_t      cursor
 ** @param uint16_t          fore_red
 ** @param uint16_t          fore_green
 ** @param uint16_t          fore_blue
 ** @param uint16_t          back_red
 ** @param uint16_t          back_green
 ** @param uint16_t          back_blue
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_recolor_cursor (xcb_connection_t *c  /**< */,
                    xcb_cursor_t      cursor  /**< */,
                    uint16_t          fore_red  /**< */,
                    uint16_t          fore_green  /**< */,
                    uint16_t          fore_blue  /**< */,
                    uint16_t          back_red  /**< */,
                    uint16_t          back_green  /**< */,
                    uint16_t          back_blue  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_RECOLOR_CURSOR,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_recolor_cursor_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.cursor = cursor;
    xcb_out.fore_red = fore_red;
    xcb_out.fore_green = fore_green;
    xcb_out.fore_blue = fore_blue;
    xcb_out.back_red = back_red;
    xcb_out.back_green = back_green;
    xcb_out.back_blue = back_blue;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_best_size_cookie_t xcb_query_best_size
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           _class
 ** @param xcb_drawable_t    drawable
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @returns xcb_query_best_size_cookie_t
 **
 *****************************************************************************/
 
xcb_query_best_size_cookie_t
xcb_query_best_size (xcb_connection_t *c  /**< */,
                     uint8_t           _class  /**< */,
                     xcb_drawable_t    drawable  /**< */,
                     uint16_t          width  /**< */,
                     uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_BEST_SIZE,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_best_size_cookie_t xcb_ret;
    xcb_query_best_size_request_t xcb_out;
    
    xcb_out._class = _class;
    xcb_out.drawable = drawable;
    xcb_out.width = width;
    xcb_out.height = height;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_best_size_cookie_t xcb_query_best_size_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           _class
 ** @param xcb_drawable_t    drawable
 ** @param uint16_t          width
 ** @param uint16_t          height
 ** @returns xcb_query_best_size_cookie_t
 **
 *****************************************************************************/
 
xcb_query_best_size_cookie_t
xcb_query_best_size_unchecked (xcb_connection_t *c  /**< */,
                               uint8_t           _class  /**< */,
                               xcb_drawable_t    drawable  /**< */,
                               uint16_t          width  /**< */,
                               uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_BEST_SIZE,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_query_best_size_cookie_t xcb_ret;
    xcb_query_best_size_request_t xcb_out;
    
    xcb_out._class = _class;
    xcb_out.drawable = drawable;
    xcb_out.width = width;
    xcb_out.height = height;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_best_size_reply_t * xcb_query_best_size_reply
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_query_best_size_cookie_t   cookie
 ** @param xcb_generic_error_t          **e
 ** @returns xcb_query_best_size_reply_t *
 **
 *****************************************************************************/
 
xcb_query_best_size_reply_t *
xcb_query_best_size_reply (xcb_connection_t              *c  /**< */,
                           xcb_query_best_size_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_query_best_size_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_query_extension_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_query_extension_request_t *_aux = (xcb_query_extension_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_query_extension_request_t);
    xcb_tmp += xcb_block_len;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_query_extension_cookie_t xcb_query_extension
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_query_extension_cookie_t
 **
 *****************************************************************************/
 
xcb_query_extension_cookie_t
xcb_query_extension (xcb_connection_t *c  /**< */,
                     uint16_t          name_len  /**< */,
                     const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_EXTENSION,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_query_extension_cookie_t xcb_ret;
    xcb_query_extension_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_extension_cookie_t xcb_query_extension_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_query_extension_cookie_t
 **
 *****************************************************************************/
 
xcb_query_extension_cookie_t
xcb_query_extension_unchecked (xcb_connection_t *c  /**< */,
                               uint16_t          name_len  /**< */,
                               const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_QUERY_EXTENSION,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_query_extension_cookie_t xcb_ret;
    xcb_query_extension_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.name_len = name_len;
    memset(xcb_out.pad1, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_query_extension_reply_t * xcb_query_extension_reply
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_query_extension_cookie_t   cookie
 ** @param xcb_generic_error_t          **e
 ** @returns xcb_query_extension_reply_t *
 **
 *****************************************************************************/
 
xcb_query_extension_reply_t *
xcb_query_extension_reply (xcb_connection_t              *c  /**< */,
                           xcb_query_extension_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_query_extension_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_list_extensions_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_list_extensions_reply_t *_aux = (xcb_list_extensions_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_list_extensions_reply_t);
    xcb_tmp += xcb_block_len;
    /* names */
    for(i=0; i<_aux->names_len; i++) {
        xcb_tmp_len = xcb_str_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_str_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_list_extensions_cookie_t xcb_list_extensions
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_list_extensions_cookie_t
 **
 *****************************************************************************/
 
xcb_list_extensions_cookie_t
xcb_list_extensions (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_LIST_EXTENSIONS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_list_extensions_cookie_t xcb_ret;
    xcb_list_extensions_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_list_extensions_cookie_t xcb_list_extensions_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_list_extensions_cookie_t
 **
 *****************************************************************************/
 
xcb_list_extensions_cookie_t
xcb_list_extensions_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_LIST_EXTENSIONS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_list_extensions_cookie_t xcb_ret;
    xcb_list_extensions_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** int xcb_list_extensions_names_length
 ** 
 ** @param const xcb_list_extensions_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_list_extensions_names_length (const xcb_list_extensions_reply_t *R  /**< */)
{
    return R->names_len;
}


/*****************************************************************************
 **
 ** xcb_str_iterator_t xcb_list_extensions_names_iterator
 ** 
 ** @param const xcb_list_extensions_reply_t *R
 ** @returns xcb_str_iterator_t
 **
 *****************************************************************************/
 
xcb_str_iterator_t
xcb_list_extensions_names_iterator (const xcb_list_extensions_reply_t *R  /**< */)
{
    xcb_str_iterator_t i;
    i.data = (xcb_str_t *) (R + 1);
    i.rem = R->names_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_list_extensions_reply_t * xcb_list_extensions_reply
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_list_extensions_cookie_t   cookie
 ** @param xcb_generic_error_t          **e
 ** @returns xcb_list_extensions_reply_t *
 **
 *****************************************************************************/
 
xcb_list_extensions_reply_t *
xcb_list_extensions_reply (xcb_connection_t              *c  /**< */,
                           xcb_list_extensions_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_list_extensions_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_change_keyboard_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_change_keyboard_mapping_request_t *_aux = (xcb_change_keyboard_mapping_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_change_keyboard_mapping_request_t);
    xcb_tmp += xcb_block_len;
    /* keysyms */
    xcb_block_len += (_aux->keycode_count * _aux->keysyms_per_keycode) * sizeof(xcb_keysym_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_keysym_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_keyboard_mapping_checked
 ** 
 ** @param xcb_connection_t   *c
 ** @param uint8_t             keycode_count
 ** @param xcb_keycode_t       first_keycode
 ** @param uint8_t             keysyms_per_keycode
 ** @param const xcb_keysym_t *keysyms
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_keyboard_mapping_checked (xcb_connection_t   *c  /**< */,
                                     uint8_t             keycode_count  /**< */,
                                     xcb_keycode_t       first_keycode  /**< */,
                                     uint8_t             keysyms_per_keycode  /**< */,
                                     const xcb_keysym_t *keysyms  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_KEYBOARD_MAPPING,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_keyboard_mapping_request_t xcb_out;
    
    xcb_out.keycode_count = keycode_count;
    xcb_out.first_keycode = first_keycode;
    xcb_out.keysyms_per_keycode = keysyms_per_keycode;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_keysym_t keysyms */
    xcb_parts[4].iov_base = (char *) keysyms;
    xcb_parts[4].iov_len = (keycode_count * keysyms_per_keycode) * sizeof(xcb_keysym_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_keyboard_mapping
 ** 
 ** @param xcb_connection_t   *c
 ** @param uint8_t             keycode_count
 ** @param xcb_keycode_t       first_keycode
 ** @param uint8_t             keysyms_per_keycode
 ** @param const xcb_keysym_t *keysyms
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_keyboard_mapping (xcb_connection_t   *c  /**< */,
                             uint8_t             keycode_count  /**< */,
                             xcb_keycode_t       first_keycode  /**< */,
                             uint8_t             keysyms_per_keycode  /**< */,
                             const xcb_keysym_t *keysyms  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_KEYBOARD_MAPPING,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_keyboard_mapping_request_t xcb_out;
    
    xcb_out.keycode_count = keycode_count;
    xcb_out.first_keycode = first_keycode;
    xcb_out.keysyms_per_keycode = keysyms_per_keycode;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_keysym_t keysyms */
    xcb_parts[4].iov_base = (char *) keysyms;
    xcb_parts[4].iov_len = (keycode_count * keysyms_per_keycode) * sizeof(xcb_keysym_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_get_keyboard_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_get_keyboard_mapping_reply_t *_aux = (xcb_get_keyboard_mapping_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_get_keyboard_mapping_reply_t);
    xcb_tmp += xcb_block_len;
    /* keysyms */
    xcb_block_len += _aux->length * sizeof(xcb_keysym_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_keysym_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_get_keyboard_mapping_cookie_t xcb_get_keyboard_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_keycode_t     first_keycode
 ** @param uint8_t           count
 ** @returns xcb_get_keyboard_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_get_keyboard_mapping_cookie_t
xcb_get_keyboard_mapping (xcb_connection_t *c  /**< */,
                          xcb_keycode_t     first_keycode  /**< */,
                          uint8_t           count  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_KEYBOARD_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_keyboard_mapping_cookie_t xcb_ret;
    xcb_get_keyboard_mapping_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.first_keycode = first_keycode;
    xcb_out.count = count;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_keyboard_mapping_cookie_t xcb_get_keyboard_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_keycode_t     first_keycode
 ** @param uint8_t           count
 ** @returns xcb_get_keyboard_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_get_keyboard_mapping_cookie_t
xcb_get_keyboard_mapping_unchecked (xcb_connection_t *c  /**< */,
                                    xcb_keycode_t     first_keycode  /**< */,
                                    uint8_t           count  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_KEYBOARD_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_keyboard_mapping_cookie_t xcb_ret;
    xcb_get_keyboard_mapping_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.first_keycode = first_keycode;
    xcb_out.count = count;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_keysym_t * xcb_get_keyboard_mapping_keysyms
 ** 
 ** @param const xcb_get_keyboard_mapping_reply_t *R
 ** @returns xcb_keysym_t *
 **
 *****************************************************************************/
 
xcb_keysym_t *
xcb_get_keyboard_mapping_keysyms (const xcb_get_keyboard_mapping_reply_t *R  /**< */)
{
    return (xcb_keysym_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_get_keyboard_mapping_keysyms_length
 ** 
 ** @param const xcb_get_keyboard_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_get_keyboard_mapping_keysyms_length (const xcb_get_keyboard_mapping_reply_t *R  /**< */)
{
    return R->length;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_get_keyboard_mapping_keysyms_end
 ** 
 ** @param const xcb_get_keyboard_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_get_keyboard_mapping_keysyms_end (const xcb_get_keyboard_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_keysym_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_get_keyboard_mapping_reply_t * xcb_get_keyboard_mapping_reply
 ** 
 ** @param xcb_connection_t                   *c
 ** @param xcb_get_keyboard_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t               **e
 ** @returns xcb_get_keyboard_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_get_keyboard_mapping_reply_t *
xcb_get_keyboard_mapping_reply (xcb_connection_t                   *c  /**< */,
                                xcb_get_keyboard_mapping_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_get_keyboard_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_change_keyboard_control_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_change_keyboard_control_request_t *_aux = (xcb_change_keyboard_control_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_change_keyboard_control_request_t);
    xcb_tmp += xcb_block_len;
    /* value_list */
    xcb_block_len += xcb_popcount(_aux->value_mask) * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_keyboard_control_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_keyboard_control_checked (xcb_connection_t *c  /**< */,
                                     uint32_t          value_mask  /**< */,
                                     const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_KEYBOARD_CONTROL,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_keyboard_control_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_keyboard_control
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          value_mask
 ** @param const uint32_t   *value_list
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_keyboard_control (xcb_connection_t *c  /**< */,
                             uint32_t          value_mask  /**< */,
                             const uint32_t   *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_KEYBOARD_CONTROL,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_keyboard_control_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.value_mask = value_mask;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_keyboard_control_cookie_t xcb_get_keyboard_control
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_keyboard_control_cookie_t
 **
 *****************************************************************************/
 
xcb_get_keyboard_control_cookie_t
xcb_get_keyboard_control (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_KEYBOARD_CONTROL,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_keyboard_control_cookie_t xcb_ret;
    xcb_get_keyboard_control_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_keyboard_control_cookie_t xcb_get_keyboard_control_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_keyboard_control_cookie_t
 **
 *****************************************************************************/
 
xcb_get_keyboard_control_cookie_t
xcb_get_keyboard_control_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_KEYBOARD_CONTROL,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_keyboard_control_cookie_t xcb_ret;
    xcb_get_keyboard_control_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_keyboard_control_reply_t * xcb_get_keyboard_control_reply
 ** 
 ** @param xcb_connection_t                   *c
 ** @param xcb_get_keyboard_control_cookie_t   cookie
 ** @param xcb_generic_error_t               **e
 ** @returns xcb_get_keyboard_control_reply_t *
 **
 *****************************************************************************/
 
xcb_get_keyboard_control_reply_t *
xcb_get_keyboard_control_reply (xcb_connection_t                   *c  /**< */,
                                xcb_get_keyboard_control_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_get_keyboard_control_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_bell_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param int8_t            percent
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_bell_checked (xcb_connection_t *c  /**< */,
                  int8_t            percent  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_BELL,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_bell_request_t xcb_out;
    
    xcb_out.percent = percent;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_bell
 ** 
 ** @param xcb_connection_t *c
 ** @param int8_t            percent
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_bell (xcb_connection_t *c  /**< */,
          int8_t            percent  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_BELL,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_bell_request_t xcb_out;
    
    xcb_out.percent = percent;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_pointer_control_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param int16_t           acceleration_numerator
 ** @param int16_t           acceleration_denominator
 ** @param int16_t           threshold
 ** @param uint8_t           do_acceleration
 ** @param uint8_t           do_threshold
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_pointer_control_checked (xcb_connection_t *c  /**< */,
                                    int16_t           acceleration_numerator  /**< */,
                                    int16_t           acceleration_denominator  /**< */,
                                    int16_t           threshold  /**< */,
                                    uint8_t           do_acceleration  /**< */,
                                    uint8_t           do_threshold  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_POINTER_CONTROL,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_change_pointer_control_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.acceleration_numerator = acceleration_numerator;
    xcb_out.acceleration_denominator = acceleration_denominator;
    xcb_out.threshold = threshold;
    xcb_out.do_acceleration = do_acceleration;
    xcb_out.do_threshold = do_threshold;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_pointer_control
 ** 
 ** @param xcb_connection_t *c
 ** @param int16_t           acceleration_numerator
 ** @param int16_t           acceleration_denominator
 ** @param int16_t           threshold
 ** @param uint8_t           do_acceleration
 ** @param uint8_t           do_threshold
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_pointer_control (xcb_connection_t *c  /**< */,
                            int16_t           acceleration_numerator  /**< */,
                            int16_t           acceleration_denominator  /**< */,
                            int16_t           threshold  /**< */,
                            uint8_t           do_acceleration  /**< */,
                            uint8_t           do_threshold  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_POINTER_CONTROL,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_change_pointer_control_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.acceleration_numerator = acceleration_numerator;
    xcb_out.acceleration_denominator = acceleration_denominator;
    xcb_out.threshold = threshold;
    xcb_out.do_acceleration = do_acceleration;
    xcb_out.do_threshold = do_threshold;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_pointer_control_cookie_t xcb_get_pointer_control
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_pointer_control_cookie_t
 **
 *****************************************************************************/
 
xcb_get_pointer_control_cookie_t
xcb_get_pointer_control (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_POINTER_CONTROL,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_pointer_control_cookie_t xcb_ret;
    xcb_get_pointer_control_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_pointer_control_cookie_t xcb_get_pointer_control_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_pointer_control_cookie_t
 **
 *****************************************************************************/
 
xcb_get_pointer_control_cookie_t
xcb_get_pointer_control_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_POINTER_CONTROL,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_pointer_control_cookie_t xcb_ret;
    xcb_get_pointer_control_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_pointer_control_reply_t * xcb_get_pointer_control_reply
 ** 
 ** @param xcb_connection_t                  *c
 ** @param xcb_get_pointer_control_cookie_t   cookie
 ** @param xcb_generic_error_t              **e
 ** @returns xcb_get_pointer_control_reply_t *
 **
 *****************************************************************************/
 
xcb_get_pointer_control_reply_t *
xcb_get_pointer_control_reply (xcb_connection_t                  *c  /**< */,
                               xcb_get_pointer_control_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_get_pointer_control_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_screen_saver_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param int16_t           timeout
 ** @param int16_t           interval
 ** @param uint8_t           prefer_blanking
 ** @param uint8_t           allow_exposures
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_screen_saver_checked (xcb_connection_t *c  /**< */,
                              int16_t           timeout  /**< */,
                              int16_t           interval  /**< */,
                              uint8_t           prefer_blanking  /**< */,
                              uint8_t           allow_exposures  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_SCREEN_SAVER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_screen_saver_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.timeout = timeout;
    xcb_out.interval = interval;
    xcb_out.prefer_blanking = prefer_blanking;
    xcb_out.allow_exposures = allow_exposures;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_screen_saver
 ** 
 ** @param xcb_connection_t *c
 ** @param int16_t           timeout
 ** @param int16_t           interval
 ** @param uint8_t           prefer_blanking
 ** @param uint8_t           allow_exposures
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_screen_saver (xcb_connection_t *c  /**< */,
                      int16_t           timeout  /**< */,
                      int16_t           interval  /**< */,
                      uint8_t           prefer_blanking  /**< */,
                      uint8_t           allow_exposures  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_SCREEN_SAVER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_screen_saver_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.timeout = timeout;
    xcb_out.interval = interval;
    xcb_out.prefer_blanking = prefer_blanking;
    xcb_out.allow_exposures = allow_exposures;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_screen_saver_cookie_t xcb_get_screen_saver
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_screen_saver_cookie_t
 **
 *****************************************************************************/
 
xcb_get_screen_saver_cookie_t
xcb_get_screen_saver (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_SCREEN_SAVER,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_screen_saver_cookie_t xcb_ret;
    xcb_get_screen_saver_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_screen_saver_cookie_t xcb_get_screen_saver_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_screen_saver_cookie_t
 **
 *****************************************************************************/
 
xcb_get_screen_saver_cookie_t
xcb_get_screen_saver_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_SCREEN_SAVER,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_screen_saver_cookie_t xcb_ret;
    xcb_get_screen_saver_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_screen_saver_reply_t * xcb_get_screen_saver_reply
 ** 
 ** @param xcb_connection_t               *c
 ** @param xcb_get_screen_saver_cookie_t   cookie
 ** @param xcb_generic_error_t           **e
 ** @returns xcb_get_screen_saver_reply_t *
 **
 *****************************************************************************/
 
xcb_get_screen_saver_reply_t *
xcb_get_screen_saver_reply (xcb_connection_t               *c  /**< */,
                            xcb_get_screen_saver_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_get_screen_saver_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_change_hosts_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_change_hosts_request_t *_aux = (xcb_change_hosts_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_change_hosts_request_t);
    xcb_tmp += xcb_block_len;
    /* address */
    xcb_block_len += _aux->address_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_hosts_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @param uint8_t           family
 ** @param uint16_t          address_len
 ** @param const uint8_t    *address
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_hosts_checked (xcb_connection_t *c  /**< */,
                          uint8_t           mode  /**< */,
                          uint8_t           family  /**< */,
                          uint16_t          address_len  /**< */,
                          const uint8_t    *address  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_HOSTS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_hosts_request_t xcb_out;
    
    xcb_out.mode = mode;
    xcb_out.family = family;
    xcb_out.pad0 = 0;
    xcb_out.address_len = address_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t address */
    xcb_parts[4].iov_base = (char *) address;
    xcb_parts[4].iov_len = address_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_change_hosts
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @param uint8_t           family
 ** @param uint16_t          address_len
 ** @param const uint8_t    *address
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_change_hosts (xcb_connection_t *c  /**< */,
                  uint8_t           mode  /**< */,
                  uint8_t           family  /**< */,
                  uint16_t          address_len  /**< */,
                  const uint8_t    *address  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_CHANGE_HOSTS,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_change_hosts_request_t xcb_out;
    
    xcb_out.mode = mode;
    xcb_out.family = family;
    xcb_out.pad0 = 0;
    xcb_out.address_len = address_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t address */
    xcb_parts[4].iov_base = (char *) address;
    xcb_parts[4].iov_len = address_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_host_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_host_t *_aux = (xcb_host_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_host_t);
    xcb_tmp += xcb_block_len;
    /* address */
    xcb_block_len += _aux->address_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** uint8_t * xcb_host_address
 ** 
 ** @param const xcb_host_t *R
 ** @returns uint8_t *
 **
 *****************************************************************************/
 
uint8_t *
xcb_host_address (const xcb_host_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_host_address_length
 ** 
 ** @param const xcb_host_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_host_address_length (const xcb_host_t *R  /**< */)
{
    return R->address_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_host_address_end
 ** 
 ** @param const xcb_host_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_host_address_end (const xcb_host_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + (R->address_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_host_next
 ** 
 ** @param xcb_host_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_host_next (xcb_host_iterator_t *i  /**< */)
{
    xcb_host_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_host_t *)(((char *)R) + xcb_host_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_host_t *) child.data;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_host_end
 ** 
 ** @param xcb_host_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_host_end (xcb_host_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_host_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_list_hosts_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_list_hosts_reply_t *_aux = (xcb_list_hosts_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_list_hosts_reply_t);
    xcb_tmp += xcb_block_len;
    /* hosts */
    for(i=0; i<_aux->hosts_len; i++) {
        xcb_tmp_len = xcb_host_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_host_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_list_hosts_cookie_t xcb_list_hosts
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_list_hosts_cookie_t
 **
 *****************************************************************************/
 
xcb_list_hosts_cookie_t
xcb_list_hosts (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_LIST_HOSTS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_list_hosts_cookie_t xcb_ret;
    xcb_list_hosts_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_list_hosts_cookie_t xcb_list_hosts_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_list_hosts_cookie_t
 **
 *****************************************************************************/
 
xcb_list_hosts_cookie_t
xcb_list_hosts_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_LIST_HOSTS,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_list_hosts_cookie_t xcb_ret;
    xcb_list_hosts_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** int xcb_list_hosts_hosts_length
 ** 
 ** @param const xcb_list_hosts_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_list_hosts_hosts_length (const xcb_list_hosts_reply_t *R  /**< */)
{
    return R->hosts_len;
}


/*****************************************************************************
 **
 ** xcb_host_iterator_t xcb_list_hosts_hosts_iterator
 ** 
 ** @param const xcb_list_hosts_reply_t *R
 ** @returns xcb_host_iterator_t
 **
 *****************************************************************************/
 
xcb_host_iterator_t
xcb_list_hosts_hosts_iterator (const xcb_list_hosts_reply_t *R  /**< */)
{
    xcb_host_iterator_t i;
    i.data = (xcb_host_t *) (R + 1);
    i.rem = R->hosts_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_list_hosts_reply_t * xcb_list_hosts_reply
 ** 
 ** @param xcb_connection_t         *c
 ** @param xcb_list_hosts_cookie_t   cookie
 ** @param xcb_generic_error_t     **e
 ** @returns xcb_list_hosts_reply_t *
 **
 *****************************************************************************/
 
xcb_list_hosts_reply_t *
xcb_list_hosts_reply (xcb_connection_t         *c  /**< */,
                      xcb_list_hosts_cookie_t   cookie  /**< */,
                      xcb_generic_error_t     **e  /**< */)
{
    return (xcb_list_hosts_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_access_control_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_access_control_checked (xcb_connection_t *c  /**< */,
                                uint8_t           mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_ACCESS_CONTROL,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_access_control_request_t xcb_out;
    
    xcb_out.mode = mode;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_access_control
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_access_control (xcb_connection_t *c  /**< */,
                        uint8_t           mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_ACCESS_CONTROL,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_access_control_request_t xcb_out;
    
    xcb_out.mode = mode;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_close_down_mode_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_close_down_mode_checked (xcb_connection_t *c  /**< */,
                                 uint8_t           mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_CLOSE_DOWN_MODE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_close_down_mode_request_t xcb_out;
    
    xcb_out.mode = mode;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_set_close_down_mode
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_set_close_down_mode (xcb_connection_t *c  /**< */,
                         uint8_t           mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_SET_CLOSE_DOWN_MODE,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_set_close_down_mode_request_t xcb_out;
    
    xcb_out.mode = mode;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_kill_client_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          resource
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_kill_client_checked (xcb_connection_t *c  /**< */,
                         uint32_t          resource  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_KILL_CLIENT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_kill_client_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.resource = resource;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_kill_client
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          resource
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_kill_client (xcb_connection_t *c  /**< */,
                 uint32_t          resource  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_KILL_CLIENT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_kill_client_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.resource = resource;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_rotate_properties_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_rotate_properties_request_t *_aux = (xcb_rotate_properties_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_rotate_properties_request_t);
    xcb_tmp += xcb_block_len;
    /* atoms */
    xcb_block_len += _aux->atoms_len * sizeof(xcb_atom_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_atom_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_rotate_properties_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param uint16_t          atoms_len
 ** @param int16_t           delta
 ** @param const xcb_atom_t *atoms
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_rotate_properties_checked (xcb_connection_t *c  /**< */,
                               xcb_window_t      window  /**< */,
                               uint16_t          atoms_len  /**< */,
                               int16_t           delta  /**< */,
                               const xcb_atom_t *atoms  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_ROTATE_PROPERTIES,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_rotate_properties_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.atoms_len = atoms_len;
    xcb_out.delta = delta;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_atom_t atoms */
    xcb_parts[4].iov_base = (char *) atoms;
    xcb_parts[4].iov_len = atoms_len * sizeof(xcb_atom_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_rotate_properties
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param uint16_t          atoms_len
 ** @param int16_t           delta
 ** @param const xcb_atom_t *atoms
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_rotate_properties (xcb_connection_t *c  /**< */,
                       xcb_window_t      window  /**< */,
                       uint16_t          atoms_len  /**< */,
                       int16_t           delta  /**< */,
                       const xcb_atom_t *atoms  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_ROTATE_PROPERTIES,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_rotate_properties_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    xcb_out.window = window;
    xcb_out.atoms_len = atoms_len;
    xcb_out.delta = delta;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_atom_t atoms */
    xcb_parts[4].iov_base = (char *) atoms;
    xcb_parts[4].iov_len = atoms_len * sizeof(xcb_atom_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_force_screen_saver_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_force_screen_saver_checked (xcb_connection_t *c  /**< */,
                                uint8_t           mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FORCE_SCREEN_SAVER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_force_screen_saver_request_t xcb_out;
    
    xcb_out.mode = mode;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_force_screen_saver
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           mode
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_force_screen_saver (xcb_connection_t *c  /**< */,
                        uint8_t           mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_FORCE_SCREEN_SAVER,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_force_screen_saver_request_t xcb_out;
    
    xcb_out.mode = mode;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_set_pointer_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_set_pointer_mapping_request_t *_aux = (xcb_set_pointer_mapping_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_set_pointer_mapping_request_t);
    xcb_tmp += xcb_block_len;
    /* map */
    xcb_block_len += _aux->map_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_set_pointer_mapping_cookie_t xcb_set_pointer_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           map_len
 ** @param const uint8_t    *map
 ** @returns xcb_set_pointer_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_set_pointer_mapping_cookie_t
xcb_set_pointer_mapping (xcb_connection_t *c  /**< */,
                         uint8_t           map_len  /**< */,
                         const uint8_t    *map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_POINTER_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_set_pointer_mapping_cookie_t xcb_ret;
    xcb_set_pointer_mapping_request_t xcb_out;
    
    xcb_out.map_len = map_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t map */
    xcb_parts[4].iov_base = (char *) map;
    xcb_parts[4].iov_len = map_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_set_pointer_mapping_cookie_t xcb_set_pointer_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           map_len
 ** @param const uint8_t    *map
 ** @returns xcb_set_pointer_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_set_pointer_mapping_cookie_t
xcb_set_pointer_mapping_unchecked (xcb_connection_t *c  /**< */,
                                   uint8_t           map_len  /**< */,
                                   const uint8_t    *map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_POINTER_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_set_pointer_mapping_cookie_t xcb_ret;
    xcb_set_pointer_mapping_request_t xcb_out;
    
    xcb_out.map_len = map_len;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t map */
    xcb_parts[4].iov_base = (char *) map;
    xcb_parts[4].iov_len = map_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_set_pointer_mapping_reply_t * xcb_set_pointer_mapping_reply
 ** 
 ** @param xcb_connection_t                  *c
 ** @param xcb_set_pointer_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t              **e
 ** @returns xcb_set_pointer_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_set_pointer_mapping_reply_t *
xcb_set_pointer_mapping_reply (xcb_connection_t                  *c  /**< */,
                               xcb_set_pointer_mapping_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_set_pointer_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_get_pointer_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_get_pointer_mapping_reply_t *_aux = (xcb_get_pointer_mapping_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_get_pointer_mapping_reply_t);
    xcb_tmp += xcb_block_len;
    /* map */
    xcb_block_len += _aux->map_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_get_pointer_mapping_cookie_t xcb_get_pointer_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_pointer_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_get_pointer_mapping_cookie_t
xcb_get_pointer_mapping (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_POINTER_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_pointer_mapping_cookie_t xcb_ret;
    xcb_get_pointer_mapping_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_pointer_mapping_cookie_t xcb_get_pointer_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_pointer_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_get_pointer_mapping_cookie_t
xcb_get_pointer_mapping_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_POINTER_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_pointer_mapping_cookie_t xcb_ret;
    xcb_get_pointer_mapping_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** uint8_t * xcb_get_pointer_mapping_map
 ** 
 ** @param const xcb_get_pointer_mapping_reply_t *R
 ** @returns uint8_t *
 **
 *****************************************************************************/
 
uint8_t *
xcb_get_pointer_mapping_map (const xcb_get_pointer_mapping_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_get_pointer_mapping_map_length
 ** 
 ** @param const xcb_get_pointer_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_get_pointer_mapping_map_length (const xcb_get_pointer_mapping_reply_t *R  /**< */)
{
    return R->map_len;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_get_pointer_mapping_map_end
 ** 
 ** @param const xcb_get_pointer_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_get_pointer_mapping_map_end (const xcb_get_pointer_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + (R->map_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_get_pointer_mapping_reply_t * xcb_get_pointer_mapping_reply
 ** 
 ** @param xcb_connection_t                  *c
 ** @param xcb_get_pointer_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t              **e
 ** @returns xcb_get_pointer_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_get_pointer_mapping_reply_t *
xcb_get_pointer_mapping_reply (xcb_connection_t                  *c  /**< */,
                               xcb_get_pointer_mapping_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_get_pointer_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_set_modifier_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_set_modifier_mapping_request_t *_aux = (xcb_set_modifier_mapping_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_set_modifier_mapping_request_t);
    xcb_tmp += xcb_block_len;
    /* keycodes */
    xcb_block_len += (_aux->keycodes_per_modifier * 8) * sizeof(xcb_keycode_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_keycode_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_set_modifier_mapping_cookie_t xcb_set_modifier_mapping
 ** 
 ** @param xcb_connection_t    *c
 ** @param uint8_t              keycodes_per_modifier
 ** @param const xcb_keycode_t *keycodes
 ** @returns xcb_set_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_set_modifier_mapping_cookie_t
xcb_set_modifier_mapping (xcb_connection_t    *c  /**< */,
                          uint8_t              keycodes_per_modifier  /**< */,
                          const xcb_keycode_t *keycodes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_MODIFIER_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_set_modifier_mapping_cookie_t xcb_ret;
    xcb_set_modifier_mapping_request_t xcb_out;
    
    xcb_out.keycodes_per_modifier = keycodes_per_modifier;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_keycode_t keycodes */
    xcb_parts[4].iov_base = (char *) keycodes;
    xcb_parts[4].iov_len = (keycodes_per_modifier * 8) * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_set_modifier_mapping_cookie_t xcb_set_modifier_mapping_unchecked
 ** 
 ** @param xcb_connection_t    *c
 ** @param uint8_t              keycodes_per_modifier
 ** @param const xcb_keycode_t *keycodes
 ** @returns xcb_set_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_set_modifier_mapping_cookie_t
xcb_set_modifier_mapping_unchecked (xcb_connection_t    *c  /**< */,
                                    uint8_t              keycodes_per_modifier  /**< */,
                                    const xcb_keycode_t *keycodes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ 0,
        /* opcode */ XCB_SET_MODIFIER_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[6];
    xcb_set_modifier_mapping_cookie_t xcb_ret;
    xcb_set_modifier_mapping_request_t xcb_out;
    
    xcb_out.keycodes_per_modifier = keycodes_per_modifier;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_keycode_t keycodes */
    xcb_parts[4].iov_base = (char *) keycodes;
    xcb_parts[4].iov_len = (keycodes_per_modifier * 8) * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_set_modifier_mapping_reply_t * xcb_set_modifier_mapping_reply
 ** 
 ** @param xcb_connection_t                   *c
 ** @param xcb_set_modifier_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t               **e
 ** @returns xcb_set_modifier_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_set_modifier_mapping_reply_t *
xcb_set_modifier_mapping_reply (xcb_connection_t                   *c  /**< */,
                                xcb_set_modifier_mapping_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_set_modifier_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_get_modifier_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_get_modifier_mapping_reply_t *_aux = (xcb_get_modifier_mapping_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_get_modifier_mapping_reply_t);
    xcb_tmp += xcb_block_len;
    /* keycodes */
    xcb_block_len += (_aux->keycodes_per_modifier * 8) * sizeof(xcb_keycode_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_keycode_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}


/*****************************************************************************
 **
 ** xcb_get_modifier_mapping_cookie_t xcb_get_modifier_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_get_modifier_mapping_cookie_t
xcb_get_modifier_mapping (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_MODIFIER_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_modifier_mapping_cookie_t xcb_ret;
    xcb_get_modifier_mapping_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_get_modifier_mapping_cookie_t xcb_get_modifier_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_get_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_get_modifier_mapping_cookie_t
xcb_get_modifier_mapping_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_GET_MODIFIER_MAPPING,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_get_modifier_mapping_cookie_t xcb_ret;
    xcb_get_modifier_mapping_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_keycode_t * xcb_get_modifier_mapping_keycodes
 ** 
 ** @param const xcb_get_modifier_mapping_reply_t *R
 ** @returns xcb_keycode_t *
 **
 *****************************************************************************/
 
xcb_keycode_t *
xcb_get_modifier_mapping_keycodes (const xcb_get_modifier_mapping_reply_t *R  /**< */)
{
    return (xcb_keycode_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_get_modifier_mapping_keycodes_length
 ** 
 ** @param const xcb_get_modifier_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_get_modifier_mapping_keycodes_length (const xcb_get_modifier_mapping_reply_t *R  /**< */)
{
    return (R->keycodes_per_modifier * 8);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_get_modifier_mapping_keycodes_end
 ** 
 ** @param const xcb_get_modifier_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_get_modifier_mapping_keycodes_end (const xcb_get_modifier_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_keycode_t *) (R + 1)) + ((R->keycodes_per_modifier * 8));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_get_modifier_mapping_reply_t * xcb_get_modifier_mapping_reply
 ** 
 ** @param xcb_connection_t                   *c
 ** @param xcb_get_modifier_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t               **e
 ** @returns xcb_get_modifier_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_get_modifier_mapping_reply_t *
xcb_get_modifier_mapping_reply (xcb_connection_t                   *c  /**< */,
                                xcb_get_modifier_mapping_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_get_modifier_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_no_operation_checked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_no_operation_checked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_NO_OPERATION,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_no_operation_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_no_operation
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_no_operation (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ 0,
        /* opcode */ XCB_NO_OPERATION,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_no_operation_request_t xcb_out;
    
    xcb_out.pad0 = 0;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


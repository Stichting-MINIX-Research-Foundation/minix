/*	$NetBSD: form.h,v 1.22 2013/11/26 01:17:00 christos Exp $	*/

/*-
 * Copyright (c) 1998-1999 Brett Lymn
 *               (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#ifndef FORM_H
#define FORM_H 1
#include <sys/queue.h>
#include <stdarg.h>
#include <curses.h>
#include <eti.h>

/* Define the types of field justification that can be used. */
#define NO_JUSTIFICATION  (0)
#define JUSTIFY_RIGHT     (1)
#define JUSTIFY_LEFT      (2)
#define JUSTIFY_CENTER    (3)

/* Define the max and min valid justification styles for range checking */
#define MIN_JUST_STYLE    NO_JUSTIFICATION
#define MAX_JUST_STYLE    JUSTIFY_CENTER

/* Options for the fields */
typedef unsigned int Form_Options;

/* form options */
#define O_BS_OVERLOAD (0x001)
#define O_NL_OVERLOAD (0x002)

/* field options */
#define O_VISIBLE  (0x001)  /* Field is visible */
#define O_ACTIVE   (0x002)  /* Field is active in the form */
#define O_PUBLIC   (0x004)  /* The contents entered into the field is echoed */
#define O_EDIT     (0x008)  /* Can edit the field */
#define O_WRAP     (0x010)  /* The field contents can line wrap */
#define O_BLANK    (0x020)  /* Blank the field on modification */
#define O_AUTOSKIP (0x040)  /* Skip to next field when current is full */
#define O_NULLOK   (0x080)  /* Field is allowed to contain no data */
#define O_STATIC   (0x100)  /* Field is not dynamic */
#define O_PASSOK   (0x200)  /* An umodified field is OK */
#define O_REFORMAT (0x400)  /* Insert newlines at linebreaks on buffer get */

/*
 * Form driver requests - be VERY careful about changing the ordering
 * of the requests below.  The form driver code depends on a particular
 * order for the requests.
 */
#define REQ_MIN_REQUEST   (KEY_MAX + 0x101) /* must equal value of the
                                               first request */

#define REQ_NEXT_PAGE     (KEY_MAX + 0x101) /* next page in form */
#define REQ_PREV_PAGE     (KEY_MAX + 0x102) /* previous page in form */
#define REQ_FIRST_PAGE    (KEY_MAX + 0x103) /* goto first page in form */
#define REQ_LAST_PAGE     (KEY_MAX + 0x104) /* goto last page in form */

#define REQ_NEXT_FIELD    (KEY_MAX + 0x105) /* move to the next field */
#define REQ_PREV_FIELD    (KEY_MAX + 0x106) /* move to the previous field */
#define REQ_FIRST_FIELD   (KEY_MAX + 0x107) /* goto the first field */
#define REQ_LAST_FIELD    (KEY_MAX + 0x108) /* goto the last field */

#define REQ_SNEXT_FIELD   (KEY_MAX + 0x109) /* move to the next field
                                               in sorted order */
#define REQ_SPREV_FIELD   (KEY_MAX + 0x10a) /* move to the prev field
                                               in sorted order */
#define REQ_SFIRST_FIELD  (KEY_MAX + 0x10b) /* move to the first
                                               sorted field */
#define REQ_SLAST_FIELD   (KEY_MAX + 0x10c) /* move to the last sorted
                                               field */

#define REQ_LEFT_FIELD    (KEY_MAX + 0x10d) /* go left one field */
#define REQ_RIGHT_FIELD   (KEY_MAX + 0x10e) /* go right one field */
#define REQ_UP_FIELD      (KEY_MAX + 0x10f) /* go up one field */
#define REQ_DOWN_FIELD    (KEY_MAX + 0x110) /* go down one field */

#define REQ_NEXT_CHAR     (KEY_MAX + 0x111) /* move to the next char
                                               in field */
#define REQ_PREV_CHAR     (KEY_MAX + 0x112) /* move to the previous
                                               char in field */
#define REQ_NEXT_LINE     (KEY_MAX + 0x113) /* go to the next line in
                                               the field */
#define REQ_PREV_LINE     (KEY_MAX + 0x114) /* go to the previous line
                                               in the field */
#define REQ_NEXT_WORD     (KEY_MAX + 0x115) /* go to the next word in
                                               the field */
#define REQ_PREV_WORD     (KEY_MAX + 0x116) /* go to the previous word
                                               in the field */
#define REQ_BEG_FIELD     (KEY_MAX + 0x117) /* go to the beginning of
                                               the field */
#define REQ_END_FIELD     (KEY_MAX + 0x118) /* go to the end of the field */
#define REQ_BEG_LINE      (KEY_MAX + 0x119) /* go to the beginning of
                                               the line */
#define REQ_END_LINE      (KEY_MAX + 0x11a) /* go to the end of the
                                               line */
#define REQ_LEFT_CHAR     (KEY_MAX + 0x11b) /* move left in the field */
#define REQ_RIGHT_CHAR    (KEY_MAX + 0x11c) /* move right in the field */
#define REQ_UP_CHAR       (KEY_MAX + 0x11d) /* move up in the field */
#define REQ_DOWN_CHAR     (KEY_MAX + 0x11e) /* move down in the field */

#define REQ_NEW_LINE      (KEY_MAX + 0x11f) /* insert/overlay a new line */
#define REQ_INS_CHAR      (KEY_MAX + 0x120) /* insert a blank char at
                                               the cursor */
#define REQ_INS_LINE      (KEY_MAX + 0x121) /* insert a blank line at
                                               the cursor */

#define REQ_DEL_CHAR      (KEY_MAX + 0x122) /* delete the current character */
#define REQ_DEL_PREV      (KEY_MAX + 0x123) /* delete the character
                                               before the current */
#define REQ_DEL_LINE      (KEY_MAX + 0x124) /* delete the current line */
#define REQ_DEL_WORD      (KEY_MAX + 0x125) /* delete the word at the cursor */
#define REQ_CLR_EOL       (KEY_MAX + 0x126) /* clear to the end of the line */
#define REQ_CLR_EOF       (KEY_MAX + 0x127) /* clear to the end of the field */
#define REQ_CLR_FIELD     (KEY_MAX + 0x128) /* clear the field */

#define REQ_OVL_MODE      (KEY_MAX + 0x129) /* overlay mode */
#define REQ_INS_MODE      (KEY_MAX + 0x12a) /* insert mode */

#define REQ_SCR_FLINE     (KEY_MAX + 0x12b) /* scroll field forward one line */
#define REQ_SCR_BLINE     (KEY_MAX + 0x12c) /* scroll field backward
                                               one line */
#define REQ_SCR_FPAGE     (KEY_MAX + 0x12d) /* scroll field forward one page */
#define REQ_SCR_BPAGE     (KEY_MAX + 0x12e) /* scroll field backward
                                               one page */
#define REQ_SCR_FHPAGE    (KEY_MAX + 0x12f) /* scroll field forward
                                               half a page */
#define REQ_SCR_BHPAGE    (KEY_MAX + 0x130) /* scroll field backward
                                               half a page */

#define REQ_SCR_FCHAR     (KEY_MAX + 0x131) /* horizontal scroll
                                               forward a character */
#define REQ_SCR_BCHAR     (KEY_MAX + 0x132) /* horizontal scroll
                                               backward a character */
#define REQ_SCR_HFLINE    (KEY_MAX + 0x133) /* horizontal scroll
                                               forward a line */
#define REQ_SCR_HBLINE    (KEY_MAX + 0x134) /* horizontal scroll
                                               backward a line */
#define REQ_SCR_HFHALF    (KEY_MAX + 0x135) /* horizontal scroll
                                               forward half a line */
#define REQ_SCR_HBHALF    (KEY_MAX + 0x136) /* horizontal scroll
                                               backward half a line */

#define REQ_VALIDATION    (KEY_MAX + 0x137) /* validate the field */
#define REQ_PREV_CHOICE   (KEY_MAX + 0x138) /* display previous field choice */
#define REQ_NEXT_CHOICE   (KEY_MAX + 0x139) /* display next field choice */

#define REQ_MAX_COMMAND   (KEY_MAX + 0x139) /* must match the last
                                               driver command */

/* The following defines are for ncurses compatibility */
#define MIN_FORM_COMMAND	REQ_MIN_REQUEST
#define MAX_FORM_COMMAND	REQ_MAX_COMMAND


typedef struct _form_string {
	size_t allocated;
	unsigned int length;
	char *string;
} FORM_STR;

typedef struct _form_field FIELD;
typedef struct _form_struct FORM;
typedef struct _form_fieldtype FIELDTYPE;

typedef struct _formi_page_struct _FORMI_PAGE_START;
typedef struct formi_type_link_struct _FORMI_TYPE_LINK;
typedef struct _formi_field_lines _FORMI_FIELD_LINES;


typedef void (*Form_Hook)(FORM *);

/* definition of a field in the form */
struct _form_field {
	unsigned int rows; /* rows in the field */
	unsigned int cols; /* columns in the field */
	unsigned int drows; /* dynamic rows */
	unsigned int dcols; /* dynamic columns */
	unsigned int max; /* maximum growth */
	unsigned int form_row; /* starting row in the form subwindow */
	unsigned int form_col; /* starting column in the form subwindow */
	unsigned int nrows; /* number of off screen rows */
	int index; /* index of this field in form fields array. */
	int nbuf; /* number of buffers associated with this field */
	int buf0_status; /* set to true if buffer 0 has changed. */
	int justification; /* justification style of the field */
	int overlay; /* set to true if field is in overlay mode */
	_FORMI_FIELD_LINES *cur_line; /* pointer to the current line cursor
					 is on */
	unsigned int start_char; /* starting char in string (horiz scroll) */
	_FORMI_FIELD_LINES *start_line; /* start line in field (vert scroll) */
	unsigned int row_count; /* number of rows actually used in field */
	unsigned int row_xpos; /* char offset of cursor in field, not same
				  as cursor_xpos due to tab expansion */
	unsigned int cursor_xpos; /* x pos of cursor in field */
	unsigned int cursor_ypos; /* y pos of cursor in field */
	short page_break; /* start of a new page on the form if 1 */
	short page; /* number of the page this field is on */
	chtype fore; /* character attributes for the foreground */
	chtype back; /* character attributes for the background */
	int pad; /* padding character */
	Form_Options opts; /* options for the field */
	FORM *parent; /* the form this field is bound to, if any */
	FIELD *up; /* field above this one */
	FIELD *down; /* field below this one */
	FIELD *left; /* field to the left of this one */
	FIELD *right; /* field to the right of this one */
	void *userptr;  /* user defined pointer. */
	FIELD *link; /* used if fields are linked */
	FIELDTYPE *type; /* type struct for the field */
	TAILQ_ENTRY(_form_field) glue; /* tail queue glue for sorting fields */
	char *args; /* args for field type. */
	_FORMI_FIELD_LINES *alines; /* array of the starts and ends of lines */
	_FORMI_FIELD_LINES *free; /* list of lines available for reuse */
	FORM_STR *buffers; /* array of buffers for the field */
};

/* define the types of fields we can have */
extern FIELDTYPE *TYPE_ALNUM;
extern FIELDTYPE *TYPE_ALPHA;
extern FIELDTYPE *TYPE_ENUM;
extern FIELDTYPE *TYPE_INTEGER;
extern FIELDTYPE *TYPE_NUMERIC;
extern FIELDTYPE *TYPE_REGEXP;
extern FIELDTYPE *TYPE_IPV4;
extern FIELDTYPE *TYPE_IPV6;
extern FIELDTYPE *TYPE_USER;

/* definition of a field type. */
struct _form_fieldtype {
	unsigned flags; /* status of the type */
	unsigned refcount; /* in use if > 0 */
	_FORMI_TYPE_LINK *link; /* set if this type is linked */

	char * (*make_args)(va_list *); /* make the args for the type */
	char * (*copy_args)(char *); /* copy the args for the type */
	void (*free_args)(char *); /* free storage used by the args */
	int (*field_check)(FIELD *, char *); /* field validation routine */
	int (*char_check)(int, char *); /* char validation routine */
	int (*next_choice)(FIELD *, char *); /* function to select next
						choice */
	int (*prev_choice)(FIELD *, char *); /* function to select prev
						choice */
};
	
/*definition of a form */

struct _form_struct {
	int in_init; /* true if performing a init or term function */
	int posted; /* the form is posted */
	int wrap; /* wrap from last field to first field if true */
	WINDOW *win; /* window for the form */
	WINDOW *subwin; /* subwindow for the form */
	WINDOW *scrwin; /* this is the window to use for output */
	void *userptr; /* user defined pointer */
	Form_Options opts; /* options for the form */
	Form_Hook form_init; /* function called when form posted and
				after page change */
	Form_Hook form_term; /* function called when form is unposted and
				before page change */
	Form_Hook field_init; /* function called when form posted and after
				 current field changes */
	Form_Hook field_term; /* function called when form unposted and
				 before current field changes */
	int field_count; /* number of fields attached */
	int cur_field; /* current field */
	int page; /* current page of form */
	int max_page; /* number of pages in the form */
	_FORMI_PAGE_START *page_starts; /* dynamic array of fields that start
					   the pages */
	TAILQ_HEAD(_formi_sort_head, _form_field) sorted_fields; /* sorted field
								list */
	FIELD **fields; /* array of fields attached to this form. */
};

/* Public function prototypes. */
__BEGIN_DECLS

FIELD       *current_field(FORM *);
int          data_ahead(FORM *);
int          data_behind(FORM *);
FIELD       *dup_field(FIELD *, int, int);
int          dynamic_field_info(FIELD *, int *, int *, int *);
char        *field_arg(FIELD *);
chtype       field_back(FIELD *);
char        *field_buffer(FIELD *, int);
int          field_count(FORM *);
chtype       field_fore(FIELD *);
int          field_index(FIELD *);
int          field_info(FIELD *, int *, int *, int *, int *, int *, int *);
Form_Hook    field_init(FORM *);
int          field_just(FIELD *);
Form_Options field_opts(FIELD *);
int          field_opts_off(FIELD *, Form_Options);
int          field_opts_on(FIELD *, Form_Options);
int          field_pad(FIELD *);
int          field_status(FIELD *);
Form_Hook    field_term(FORM *);
FIELDTYPE   *field_type(FIELD *);
void        *field_userptr(FIELD *);
int          form_driver(FORM *, int);
FIELD      **form_fields(FORM *);
Form_Hook    form_init(FORM *);
int          form_max_page(FORM *);
Form_Options form_opts(FORM *);
int          form_opts_off(FORM *, Form_Options);
int          form_opts_on(FORM *, Form_Options);
int          form_page(FORM *);
WINDOW      *form_sub(FORM *);
Form_Hook    form_term(FORM *);
void        *form_userptr(FORM *);
WINDOW      *form_win(FORM *);
int          free_field(FIELD *);
int          free_fieldtype(FIELDTYPE *);
int          free_form(FORM *);
FIELD       *link_field(FIELD *, int, int);
FIELDTYPE   *link_fieldtype(FIELDTYPE *, FIELDTYPE *);
int          move_field(FIELD *, int, int);
FIELD       *new_field(int, int, int, int, int, int);
FIELDTYPE   *new_fieldtype(int (* field_check)(FIELD *, char *),
			   int (* char_check)(int, char *));
FORM        *new_form(FIELD **);
int          new_page(FIELD *);
int          pos_form_cursor(FORM *);
int          post_form(FORM *);
int          scale_form(FORM *, int *, int *);
int          set_current_field(FORM *, FIELD *);
int          set_field_back(FIELD *, chtype);
int          set_field_buffer(FIELD *, int, char *);
int          set_field_fore(FIELD *, chtype);
int          set_field_init(FORM *, Form_Hook);
int          set_field_just(FIELD *, int);
int          set_field_opts(FIELD *, Form_Options);
int          set_field_pad(FIELD *, int);
int          set_field_printf(FIELD *, int, char *, ...) __printflike(3, 4);
int          set_field_status(FIELD *, int);
int          set_field_term(FORM *, Form_Hook);
int          set_field_type(FIELD *, FIELDTYPE *, ...);
int          set_field_userptr(FIELD *, void *);
int          set_fieldtype_arg(FIELDTYPE *, char *(*)(va_list *),
			       char *(*)(char *),
			       void (*)(char *));
int          set_fieldtype_choice(FIELDTYPE *, int (*)(FIELD *, char *),
				  int (*)(FIELD *, char *));
int          set_form_fields(FORM *, FIELD **);
int          set_form_init(FORM *, Form_Hook);
int          set_form_opts(FORM *, Form_Options);
int          set_form_page(FORM *, int);
int          set_form_sub(FORM *, WINDOW *);
int          set_form_term(FORM *, Form_Hook);
int          set_form_userptr(FORM *, void *);
int          set_form_win(FORM *, WINDOW *);
int          set_max_field(FIELD *, int);
int          set_new_page(FIELD *, int);
int          unpost_form(FORM *);

__END_DECLS

#endif /* FORM_H */

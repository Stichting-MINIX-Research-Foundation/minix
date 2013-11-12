/*	$NetBSD: field_types.c,v 1.7 2006/03/19 20:02:27 christos Exp $	*/

/*-
 * Copyright (c) 1998-1999 Brett Lymn
 *                         (blymn@baea.com.au, brett_lymn@yahoo.com.au)
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: field_types.c,v 1.7 2006/03/19 20:02:27 christos Exp $");

#include <stdlib.h>
#include <stdarg.h>
#include "form.h"
#include "internals.h"

extern FIELD _formi_default_field;

/* function prototypes.... */
static void
_formi_create_field_args(FIELDTYPE *type, char **type_args,
			 formi_type_link **link, va_list *args, int *error);
static FIELDTYPE *
_formi_create_fieldtype(void);

/*
 * Process the arguments, if any, for the field type.
 */
static void
_formi_create_field_args(FIELDTYPE *type, char **type_args,
			 formi_type_link **link, va_list *args, int *error)
{
	formi_type_link *l;

	l = NULL;
	if ((type != NULL)
	    && ((type->flags & _TYPE_HAS_ARGS) == _TYPE_HAS_ARGS)) {
		if ((type->flags & _TYPE_IS_LINKED) == _TYPE_IS_LINKED) {
			l = malloc(sizeof(*l));
			if (l != NULL) {
				_formi_create_field_args(type->link->next,
							 type_args,
							 &type->link->next->link,
							 args,
							 error);
				_formi_create_field_args(type->link->prev,
							 type_args,
							 &type->link->prev->link,
							 args,
							 error);
				(*link) = l;
			}

			(*error)++;
		} else {
			if ((*type_args = (char *) type->make_args(args))
			    == NULL)
				(*error)++;
		}
	}
}

/*
 * Allocate a new fieldtype structure, initialise it and return the
 * struct to the caller.
 */
static FIELDTYPE *
_formi_create_fieldtype(void)
{
	FIELDTYPE *new;
	
	if ((new = malloc(sizeof(*new))) == NULL)
		return NULL;

	new->flags = _TYPE_NO_FLAGS;
	new->refcount = 0;
	new->link = NULL;
	new->make_args = NULL;
	new->copy_args = NULL;
	new->free_args = NULL;
	new->field_check = NULL;
	new->char_check = NULL;
	new->next_choice = NULL;
	new->prev_choice = NULL;

	return new;
}

/*
 * Set the field type of the field to be the one given.
 */
int
set_field_type(FIELD *fptr, FIELDTYPE *type, ...)
{
	va_list args;
	FIELD *field;
	int error = 0;

	va_start(args, type);
	
	field = (fptr == NULL)? &_formi_default_field : fptr;

	field->type = type;
	_formi_create_field_args(type, &field->args, &type->link, &args,
				 &error);
	va_end(args);
	
	if (error)
		return E_BAD_ARGUMENT;
		
	return E_OK;
}

/*
 * Return the field type associated with the given field
 */
FIELDTYPE *
field_type(FIELD *fptr)
{
	FIELD *field;
	
	field = (fptr == NULL)? &_formi_default_field : fptr;

	return field->type;
}


/*
 * Return the field arguments for the given field.
 */
char *
field_arg(FIELD *fptr)
{
	FIELD *field;

	field = (fptr == NULL)? &_formi_default_field : fptr;

	return field->args;
}

/*
 * Create a new field type.  Caller must specify a field_check routine
 * and char_check routine.
 */
FIELDTYPE *
new_fieldtype(int (*field_check)(FIELD *, char *),
	      int (*char_check)(int, char *))
{
	FIELDTYPE *new;
	
	if ((field_check == NULL) && (char_check == NULL))
		return NULL;

	if ((new = _formi_create_fieldtype()) != NULL) {
		new->field_check = field_check;
		new->char_check = char_check;
	}

	return new;
}

/*
 * Free the storage used by the fieldtype.
 */
int
free_fieldtype(FIELDTYPE *fieldtype)
{
	if (fieldtype == NULL)
		return E_BAD_ARGUMENT;

	if (fieldtype->refcount > 0)
		return E_CONNECTED;

	if ((fieldtype->flags & _TYPE_IS_BUILTIN) == _TYPE_IS_BUILTIN)
		return E_BAD_ARGUMENT; /* don't delete builtin types! */

	if ((fieldtype->flags & _TYPE_IS_LINKED) == _TYPE_IS_LINKED) 
	{
		fieldtype->link->next->refcount--;
		fieldtype->link->prev->refcount--;
	}
	
	free(fieldtype);

	return E_OK;
}

/*
 * Set the field type arguments for the given field type.
 */
int
set_fieldtype_arg(FIELDTYPE *fieldtype, char * (*make_args)(va_list *),
		  char * (*copy_args)(char*), void (*free_args)(char *))
{
	if ((fieldtype == NULL) || (make_args == NULL)
	    || (copy_args == NULL) || (free_args == NULL))
		return E_BAD_ARGUMENT;

	fieldtype->make_args = make_args;
	fieldtype->copy_args = copy_args;
	fieldtype->free_args = free_args;

	return E_OK;
}

/*
 * Set up the choice list functions for the given fieldtype.
 */
int
set_fieldtype_choice(FIELDTYPE *fieldtype, int (*next_choice)(FIELD *, char *),
		     int (*prev_choice)(FIELD *, char *))
{
	if ((fieldtype == NULL) || (next_choice == NULL)
	    || (prev_choice == NULL))
		return E_BAD_ARGUMENT;

	fieldtype->next_choice = next_choice;
	fieldtype->prev_choice = prev_choice;

	return E_OK;
}

/*
 * Link the two given types to produce a new type, return this new type.
 */
FIELDTYPE *
link_fieldtype(FIELDTYPE *type1, FIELDTYPE *type2)
{
	FIELDTYPE *new;

	if ((type1 == NULL) || (type2 == NULL))
		return NULL;
	
	if ((new = _formi_create_fieldtype()) == NULL)
		return NULL;

	new->flags = _TYPE_IS_LINKED;
	new->flags |= ((type1->flags & _TYPE_HAS_ARGS)
		       | (type2->flags & _TYPE_HAS_ARGS));
	if ((new->link = malloc(sizeof(*new->link))) == NULL) {
		free(new);
		return NULL;
	}
	
	new->link->prev = type1;
	new->link->next = type2;
	type1->refcount++;
	type2->refcount++;
	
	return new;
}

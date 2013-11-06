/*	$NetBSD: avl.c,v 1.7 2005/02/11 06:21:22 simonb Exp $	*/

/*
 * Copyright (c) 1997 Philip A. Nelson.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 * NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS;  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY,  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* avl.c: Routines for manipulation an avl tree. 
 *
 * an include file should define the following minimum struct.:
 * (Comments must be made into real comments.)
 *
 *	typedef struct id_rec {
 *		/ * The balanced binary tree fields. * /
 *		char  *id;      / * The name. * /
 *		short balance;  / * For the balanced tree. * /
 *		struct id_rec *left, *right; / * Tree pointers. * /
 *
 *		/ * Other information fields. * /
 *	} id_rec;
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: avl.c,v 1.7 2005/02/11 06:21:22 simonb Exp $");
#endif


#include <string.h>

#include "defs.h"

/*  find_id returns a pointer to node in TREE that has the correct
    ID.  If there is no node in TREE with ID, NULL is returned. */

id_rec *
  find_id (id_rec *tree, char *id)
{
  int cmp_result;
  
  /* Check for an empty tree. */
  if (tree == NULL)
    return NULL;
  
  /* Recursively search the tree. */
  cmp_result = strcmp (id, tree->id);
  if (cmp_result == 0)
    return tree;  /* This is the item. */
  else if (cmp_result < 0)
    return find_id (tree->left, id);
  else
    return find_id (tree->right, id);  
}


/* insert_id inserts a NEW_ID rec into the tree whose ROOT is
   provided.  insert_id returns TRUE if the tree height from
   ROOT down is increased otherwise it returns FALSE.  This is a
   recursive balanced binary tree insertion algorithm. */

int insert_id (id_rec **root, id_rec *new_id)
{
  id_rec *A, *B;
  
  /* If root is NULL, this where it is to be inserted. */
  if (*root == NULL)
    {
      *root = new_id;
      new_id->left = NULL;
      new_id->right = NULL;
      new_id->balance = 0;
      return (TRUE);
    }
  
  /* We need to search for a leaf. */
  if (strcmp (new_id->id, (*root)->id) < 0)
    {
      /* Insert it on the left. */
      if (insert_id (&((*root)->left), new_id))
	{
	  /* The height increased. */
	  (*root)->balance--;
	  
	  switch ((*root)->balance)
	    {
	    case  0:  /* no height increase. */
	      return (FALSE);
	    case -1:  /* height increase. */
	      return (TRUE);
	    case -2:  /* we need to do a rebalancing act. */
	      A = *root;
	      B = (*root)->left;
	      if (B->balance <= 0)
		{
		  /* Single Rotate. */
		  A->left = B->right;
		  B->right = A;
		  *root = B;
		  A->balance = 0;
		  B->balance = 0;
		}
	      else
		{
		  /* Double Rotate. */
		  *root = B->right;
		  B->right = (*root)->left;
		  A->left = (*root)->right;
		  (*root)->left = B;
		  (*root)->right = A;
		  switch ((*root)->balance)
		    {
		    case -1:
		      A->balance = 1;
		      B->balance = 0;
		      break;
		    case  0:
		      A->balance = 0;
		      B->balance = 0;
		      break;
		    case  1:
		      A->balance = 0;
		      B->balance = -1;
		      break;
		    }
		  (*root)->balance = 0;
		}
	    }     
	} 
    }
  else
    {
      /* Insert it on the right. */
      if (insert_id (&((*root)->right), new_id))
	{
	  /* The height increased. */
	  (*root)->balance++;
	  switch ((*root)->balance)
	    {
	    case 0:  /* no height increase. */
	      return (FALSE);
	    case 1:  /* height increase. */
	      return (TRUE);
	    case 2:  /* we need to do a rebalancing act. */
	      A = *root;
	      B = (*root)->right;
	      if (B->balance >= 0)
		{
		  /* Single Rotate. */
		  A->right = B->left;
		  B->left = A;
		  *root = B;
		  A->balance = 0;
		  B->balance = 0;
		}
	      else
		{
		  /* Double Rotate. */
		  *root = B->left;
		  B->left = (*root)->right;
		  A->right = (*root)->left;
		  (*root)->left = A;
		  (*root)->right = B;
		  switch ((*root)->balance)
		    {
		    case -1:
		      A->balance = 0;
		      B->balance = 1;
		      break;
		    case  0:
		      A->balance = 0;
		      B->balance = 0;
		      break;
		    case  1:
		      A->balance = -1;
		      B->balance = 0;
		      break;
		    }
		  (*root)->balance = 0;
		}
	    }     
	} 
    }
  
  /* If we fall through to here, the tree did not grow in height. */
  return (FALSE);
}

/*	$NetBSD: chfs_readinode.c,v 1.9 2014/09/01 16:31:17 he Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2010 David Tengeri <dtengeri@inf.u-szeged.hu>
 * Copyright (C) 2010 Tamas Toth <ttoth@inf.u-szeged.hu>
 * Copyright (C) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/buf.h>

#include "chfs.h"

/* tmp node operations */
int chfs_check_td_data(struct chfs_mount *,
    struct chfs_tmp_dnode *);
int chfs_check_td_node(struct chfs_mount *,
    struct chfs_tmp_dnode *);
struct chfs_node_ref *chfs_first_valid_data_ref(struct chfs_node_ref *);
int chfs_add_tmp_dnode_to_tree(struct chfs_mount *,
    struct chfs_readinode_info *,
    struct chfs_tmp_dnode *);
void chfs_add_tmp_dnode_to_tdi(struct chfs_tmp_dnode_info *,
	struct chfs_tmp_dnode *);
void chfs_remove_tmp_dnode_from_tdi(struct chfs_tmp_dnode_info *,
	struct chfs_tmp_dnode *);
static void chfs_kill_td(struct chfs_mount *,
    struct chfs_tmp_dnode *);
static void chfs_kill_tdi(struct chfs_mount *,
    struct chfs_tmp_dnode_info *);
/* frag node operations */
struct chfs_node_frag *new_fragment(struct chfs_full_dnode *,
    uint32_t,
    uint32_t);
int no_overlapping_node(struct rb_tree *, struct chfs_node_frag *,
    struct chfs_node_frag *, uint32_t);
int chfs_add_frag_to_fragtree(struct chfs_mount *,
    struct rb_tree *,
    struct chfs_node_frag *);
void chfs_obsolete_node_frag(struct chfs_mount *,
    struct chfs_node_frag *);
/* general node operations */
int chfs_get_data_nodes(struct chfs_mount *,
    struct chfs_inode *,
    struct chfs_readinode_info *);
int chfs_build_fragtree(struct chfs_mount *,
    struct chfs_inode *,
    struct chfs_readinode_info *);



/* tmp node rbtree operations */
static signed int
tmp_node_compare_nodes(void *ctx, const void *n1, const void *n2)
{
	const struct chfs_tmp_dnode_info *tdi1 = n1;
	const struct chfs_tmp_dnode_info *tdi2 = n2;

	return (tdi1->tmpnode->node->ofs - tdi2->tmpnode->node->ofs);
}

static signed int
tmp_node_compare_key(void *ctx, const void *n, const void *key)
{
	const struct chfs_tmp_dnode_info *tdi = n;
	uint64_t ofs =  *(const uint64_t *)key;

	return (tdi->tmpnode->node->ofs - ofs);
}

const rb_tree_ops_t tmp_node_rbtree_ops = {
	.rbto_compare_nodes = tmp_node_compare_nodes,
	.rbto_compare_key = tmp_node_compare_key,
	.rbto_node_offset = offsetof(struct chfs_tmp_dnode_info, rb_node),
	.rbto_context = NULL
};


/* frag node rbtree operations */
static signed int
frag_compare_nodes(void *ctx, const void *n1, const void *n2)
{
	const struct chfs_node_frag *frag1 = n1;
	const struct chfs_node_frag *frag2 = n2;

	return (frag1->ofs - frag2->ofs);
}

static signed int
frag_compare_key(void *ctx, const void *n, const void *key)
{
	const struct chfs_node_frag *frag = n;
	uint64_t ofs = *(const uint64_t *)key;

	return (frag->ofs - ofs);
}

const rb_tree_ops_t frag_rbtree_ops = {
	.rbto_compare_nodes = frag_compare_nodes,
	.rbto_compare_key   = frag_compare_key,
	.rbto_node_offset = offsetof(struct chfs_node_frag, rb_node),
	.rbto_context = NULL
};


/*
 * chfs_check_td_data - checks the data CRC of the node
 *
 * Returns: 0 - if everything OK;
 * 	    	1 - if CRC is incorrect;
 * 	    	2 - else;
 *	    	error code if an error occured.
 */
int
chfs_check_td_data(struct chfs_mount *chmp,
    struct chfs_tmp_dnode *td)
{
	int err;
	size_t retlen, len, totlen;
	uint32_t crc;
	uint64_t ofs;
	char *buf;
	struct chfs_node_ref *nref = td->node->nref;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(!mutex_owned(&chmp->chm_lock_sizes));

	ofs = CHFS_GET_OFS(nref->nref_offset) + sizeof(struct chfs_flash_data_node);
	len = td->node->size;
	if (!len)
		return 0;

	/* Read data. */
	buf = kmem_alloc(len, KM_SLEEP);
	if (!buf) {
		dbg("allocating error\n");
		return 2;
	}
	err = chfs_read_leb(chmp, nref->nref_lnr, buf, ofs, len, &retlen);
	if (err) {
		dbg("error while reading: %d\n", err);
		err = 2;
		goto out;
	}

	/* Check crc. */
	if (len != retlen) {
		dbg("len:%zu, retlen:%zu\n", len, retlen);
		err = 2;
		goto out;
	}
	crc = crc32(0, (uint8_t *)buf, len);

	if (crc != td->data_crc) {
		dbg("crc failed, calculated: 0x%x, orig: 0x%x\n", crc, td->data_crc);
		kmem_free(buf, len);
		return 1;
	}

	/* Correct sizes. */
	CHFS_MARK_REF_NORMAL(nref);
	totlen = CHFS_PAD(sizeof(struct chfs_flash_data_node) + len);

	mutex_enter(&chmp->chm_lock_sizes);
	chfs_change_size_unchecked(chmp, &chmp->chm_blocks[nref->nref_lnr], -totlen);
	chfs_change_size_used(chmp, &chmp->chm_blocks[nref->nref_lnr], totlen);
	mutex_exit(&chmp->chm_lock_sizes);
	KASSERT(chmp->chm_blocks[nref->nref_lnr].used_size <= chmp->chm_ebh->eb_size);

	err = 0;
out:
	kmem_free(buf, len);
	return err;
}

/* chfs_check_td_node - checks a temporary node */
int
chfs_check_td_node(struct chfs_mount *chmp, struct chfs_tmp_dnode *td)
{
	int ret;

	if (CHFS_REF_FLAGS(td->node->nref) != CHFS_UNCHECKED_NODE_MASK)
		return 0;

	ret = chfs_check_td_data(chmp, td);
	return ret;
}

/* 
 * chfs_first_valid_data_ref -
 * returns the first valid nref after the given nref
 */
struct chfs_node_ref *
chfs_first_valid_data_ref(struct chfs_node_ref *nref)
{
	while (nref) {
		if (!CHFS_REF_OBSOLETE(nref)) {
#ifdef DGB_MSG_GC
			if (nref->nref_lnr == REF_EMPTY_NODE) {
				dbg("FIRST VALID IS EMPTY!\n");
			}
#endif
			return nref;
		}

		if (nref->nref_next) {
			nref = nref->nref_next;
		} else
			break;
	}
	return NULL;
}

/*
 * chfs_add_tmp_dnode_to_tdi -
 * adds a temporary node to a temporary node descriptor
 */
void
chfs_add_tmp_dnode_to_tdi(struct chfs_tmp_dnode_info *tdi,
	struct chfs_tmp_dnode *td)
{
	if (!tdi->tmpnode) {
	/* The chain is empty. */
		tdi->tmpnode = td;
	} else {
	/* Insert into the chain. */
		struct chfs_tmp_dnode *tmp = tdi->tmpnode;
		while (tmp->next) {
			tmp = tmp->next;
		}
		tmp->next = td;
	}
}

/*
 * chfs_remove_tmp_dnode_from_tdi - 
 * removes a temporary node from its descriptor
 */
void
chfs_remove_tmp_dnode_from_tdi(struct chfs_tmp_dnode_info *tdi,
	struct chfs_tmp_dnode *td)
{
	if (tdi->tmpnode == td) {
	/* It's the first in the chain. */
		tdi->tmpnode = tdi->tmpnode->next;
	} else {
	/* Remove from the middle of the chain. */
		struct chfs_tmp_dnode *tmp = tdi->tmpnode->next;
		while (tmp->next && tmp->next != td) {
			tmp = tmp->next;
		}
		if (tmp->next) {
			tmp->next = td->next;
		}
	}
}

/* chfs_kill_td - removes all components of a temporary node */
static void
chfs_kill_td(struct chfs_mount *chmp,
    struct chfs_tmp_dnode *td)
{
	struct chfs_vnode_cache *vc;
	if (td->node) {
		mutex_enter(&chmp->chm_lock_vnocache);
		/* Remove the node from the vnode cache's data node chain. */
		vc = chfs_nref_to_vc(td->node->nref);
		chfs_remove_and_obsolete(chmp, vc, td->node->nref, &vc->dnode);	
		mutex_exit(&chmp->chm_lock_vnocache);
	}

	chfs_free_tmp_dnode(td);
}

/* chfs_kill_tdi - removes a temporary node descriptor */
static void
chfs_kill_tdi(struct chfs_mount *chmp,
    struct chfs_tmp_dnode_info *tdi)
{
	struct chfs_tmp_dnode *next, *tmp = tdi->tmpnode;

	/* Iterate the chain and remove all temporary node from it. */
	while (tmp) {
		next = tmp->next;
		chfs_kill_td(chmp, tmp);
		tmp = next;
	}

	chfs_free_tmp_dnode_info(tdi);
}

/* 
 * chfs_add_tmp_dnode_to_tree - 
 * adds a temporary node to the temporary tree
 */
int
chfs_add_tmp_dnode_to_tree(struct chfs_mount *chmp,
    struct chfs_readinode_info *rii,
    struct chfs_tmp_dnode *newtd)
{
	uint64_t end_ofs = newtd->node->ofs + newtd->node->size;
	struct chfs_tmp_dnode_info *this;
	struct rb_node *node, *prev_node;
	struct chfs_tmp_dnode_info *newtdi;

	node = rb_tree_find_node(&rii->tdi_root, &newtd->node->ofs);
	if (node) {
		this = (struct chfs_tmp_dnode_info *)node;
		while (this->tmpnode->overlapped) {
			prev_node = rb_tree_iterate(&rii->tdi_root, node, RB_DIR_LEFT);
			if (!prev_node) {
				this->tmpnode->overlapped = 0;
				break;
			}
			node = prev_node;
			this = (struct chfs_tmp_dnode_info *)node;
		}
	}

	while (node) {
		this = (struct chfs_tmp_dnode_info *)node;
		if (this->tmpnode->node->ofs > end_ofs)
			break;
		
		struct chfs_tmp_dnode *tmp_td = this->tmpnode;
		while (tmp_td) {
			if (tmp_td->version == newtd->version) {
				/* This is a new version of an old node. */
				if (!chfs_check_td_node(chmp, tmp_td)) {
					dbg("calling kill td 0\n");
					chfs_kill_td(chmp, newtd);
					return 0;
				} else {
					chfs_remove_tmp_dnode_from_tdi(this, tmp_td);
					chfs_kill_td(chmp, tmp_td);
					chfs_add_tmp_dnode_to_tdi(this, newtd);
					return 0;
				}
			}
			if (tmp_td->version < newtd->version &&
				tmp_td->node->ofs >= newtd->node->ofs &&
				tmp_td->node->ofs + tmp_td->node->size <= end_ofs) {
				/* New node entirely overlaps 'this' */
				if (chfs_check_td_node(chmp, newtd)) {
					dbg("calling kill td 2\n");
					chfs_kill_td(chmp, newtd);
					return 0;
				}
				/* ... and is good. Kill 'this' and any subsequent nodes which are also overlapped */
				while (tmp_td && tmp_td->node->ofs + tmp_td->node->size <= end_ofs) {
					struct rb_node *next = rb_tree_iterate(&rii->tdi_root, this, RB_DIR_RIGHT);
					struct chfs_tmp_dnode_info *next_tdi = (struct chfs_tmp_dnode_info *)next;
					struct chfs_tmp_dnode *next_td = NULL;
					if (tmp_td->next) {
						next_td = tmp_td->next;
					} else if (next_tdi) {
						next_td = next_tdi->tmpnode;
					}
					if (tmp_td->version < newtd->version) {
						chfs_remove_tmp_dnode_from_tdi(this, tmp_td);
						chfs_kill_td(chmp, tmp_td);
						if (!this->tmpnode) {
							rb_tree_remove_node(&rii->tdi_root, this);
							chfs_kill_tdi(chmp, this);
							this = next_tdi;
						}
					}
					tmp_td = next_td;
				}
				continue;
			}
			if (tmp_td->version > newtd->version &&
				tmp_td->node->ofs <= newtd->node->ofs &&
				tmp_td->node->ofs + tmp_td->node->size >= end_ofs) {
				/* New node entirely overlapped by 'this' */
				if (!chfs_check_td_node(chmp, tmp_td)) {
					dbg("this version: %llu\n",
						(unsigned long long)tmp_td->version);
					dbg("this ofs: %llu, size: %u\n",
						(unsigned long long)tmp_td->node->ofs,
						tmp_td->node->size);
					dbg("calling kill td 4\n");
					chfs_kill_td(chmp, newtd);
					return 0;
				}
				/* ... but 'this' was bad. Replace it... */
				chfs_remove_tmp_dnode_from_tdi(this, tmp_td);
				chfs_kill_td(chmp, tmp_td);
				if (!this->tmpnode) {
					rb_tree_remove_node(&rii->tdi_root, this);
					chfs_kill_tdi(chmp, this);
				}
				dbg("calling kill td 5\n");
				chfs_kill_td(chmp, newtd);
				break;
			}
			tmp_td = tmp_td->next;
		}
		node = rb_tree_iterate(&rii->tdi_root, node, RB_DIR_RIGHT);
	}

	newtdi = chfs_alloc_tmp_dnode_info();
	chfs_add_tmp_dnode_to_tdi(newtdi, newtd);
	/* We neither completely obsoleted nor were completely
	   obsoleted by an earlier node. Insert into the tree */
	struct chfs_tmp_dnode_info *tmp_tdi = rb_tree_insert_node(&rii->tdi_root, newtdi);
	if (tmp_tdi != newtdi) {
		chfs_remove_tmp_dnode_from_tdi(newtdi, newtd);
		chfs_add_tmp_dnode_to_tdi(tmp_tdi, newtd);
		chfs_kill_tdi(chmp, newtdi);
	}

	/* If there's anything behind that overlaps us, note it */
	node = rb_tree_iterate(&rii->tdi_root, node, RB_DIR_LEFT);
	if (node) {
		while (1) {
			this = (struct chfs_tmp_dnode_info *)node;
			if (this->tmpnode->node->ofs + this->tmpnode->node->size > newtd->node->ofs) {
				newtd->overlapped = 1;
			}
			if (!this->tmpnode->overlapped)
				break;

			prev_node = rb_tree_iterate(&rii->tdi_root, node, RB_DIR_LEFT);
			if (!prev_node) {
				this->tmpnode->overlapped = 0;
				break;
			}
			node = prev_node;
		}
	}

	/* If the new node overlaps anything ahead, note it */
	node = rb_tree_iterate(&rii->tdi_root, node, RB_DIR_RIGHT);
	this = (struct chfs_tmp_dnode_info *)node;
	while (this && this->tmpnode->node->ofs < end_ofs) {
		this->tmpnode->overlapped = 1;
		node = rb_tree_iterate(&rii->tdi_root, node, RB_DIR_RIGHT);
		this = (struct chfs_tmp_dnode_info *)node;
	}
	return 0;
}


/* new_fragment - creates a new fragment for a data node */
struct chfs_node_frag *
new_fragment(struct chfs_full_dnode *fdn, uint32_t ofs, uint32_t size)
{
	struct chfs_node_frag *newfrag;
	newfrag = chfs_alloc_node_frag();
	if (newfrag) {
		/* Initialize fragment. */
		newfrag->ofs = ofs;
		newfrag->size = size;
		newfrag->node = fdn;
		if (newfrag->node) {
			newfrag->node->frags++;
		}
	} else {
		chfs_err("cannot allocate a chfs_node_frag object\n");
	}
	return newfrag;
}

/*
 * no_overlapping_node - inserts a node to the fragtree
 * Puts hole frag into the holes between fragments.
 */
int
no_overlapping_node(struct rb_tree *fragtree,
    struct chfs_node_frag *newfrag,
    struct chfs_node_frag *this, uint32_t lastend)
{
	if (lastend < newfrag->node->ofs) {
		struct chfs_node_frag *holefrag;

		holefrag = new_fragment(NULL, lastend, newfrag->node->ofs - lastend);
		if (!holefrag) {
			chfs_free_node_frag(newfrag);
			return ENOMEM;
		}

		rb_tree_insert_node(fragtree, holefrag);
	}

	rb_tree_insert_node(fragtree, newfrag);

	return 0;
}

/*
 * chfs_add_frag_to_fragtree - 
 * adds a fragment to a data node's fragtree
 */
int
chfs_add_frag_to_fragtree(struct chfs_mount *chmp,
    struct rb_tree *fragtree,
    struct chfs_node_frag *newfrag)
{
	struct chfs_node_frag *this;
	uint32_t lastend;
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	/* Find the offset of frag which is before the new one. */
	this = (struct chfs_node_frag *)rb_tree_find_node_leq(fragtree, &newfrag->ofs);

	if (this) {
		lastend = this->ofs + this->size;
	} else {
		lastend = 0;
	}

	/* New fragment is end of the file and there is no overlapping. */
	if (lastend <= newfrag->ofs) {
		if (lastend && (lastend - 1) >> PAGE_SHIFT == newfrag->ofs >> PAGE_SHIFT) {
			if (this->node)
				CHFS_MARK_REF_NORMAL(this->node->nref);
			CHFS_MARK_REF_NORMAL(newfrag->node->nref);
		}
		return no_overlapping_node(fragtree, newfrag, this, lastend);
	}

	if (newfrag->ofs > this->ofs) {
		CHFS_MARK_REF_NORMAL(newfrag->node->nref);
		if (this->node)
			CHFS_MARK_REF_NORMAL(this->node->nref);

		if (this->ofs + this->size > newfrag->ofs + newfrag->size) {
			/* Newfrag is inside of this. */
			struct chfs_node_frag *newfrag2;

			newfrag2 = new_fragment(this->node, newfrag->ofs + newfrag->size,
			    this->ofs + this->size - newfrag->ofs - newfrag->size);
			if (!newfrag2)
				return ENOMEM;

			this->size = newfrag->ofs - this->ofs;

			rb_tree_insert_node(fragtree, newfrag);
			rb_tree_insert_node(fragtree, newfrag2);

			return 0;
		}
		/* Newfrag is bottom of this. */
		this->size = newfrag->ofs - this->ofs;
		rb_tree_insert_node(fragtree, newfrag);
	} else {
		/* Newfrag start at same point */
		//TODO replace instead of remove and insert
		rb_tree_remove_node(fragtree, this);
		rb_tree_insert_node(fragtree, newfrag);

		if (newfrag->ofs + newfrag->size >= this->ofs+this->size) {
			chfs_obsolete_node_frag(chmp, this);
		} else {
			this->ofs += newfrag->size;
			this->size -= newfrag->size;

			rb_tree_insert_node(fragtree, this);
			return 0;
		}
	}
	/* OK, now we have newfrag added in the correct place in the tree, but
	   frag_next(newfrag) may be a fragment which is overlapped by it
	*/
	while ((this = frag_next(fragtree, newfrag)) && newfrag->ofs + newfrag->size >= this->ofs + this->size) {
		rb_tree_remove_node(fragtree, this);
		chfs_obsolete_node_frag(chmp, this);
	}

	if (!this || newfrag->ofs + newfrag->size == this->ofs)
		return 0;

	this->size = (this->ofs + this->size) - (newfrag->ofs + newfrag->size);
	this->ofs = newfrag->ofs + newfrag->size;

	if (this->node)
		CHFS_MARK_REF_NORMAL(this->node->nref);
	CHFS_MARK_REF_NORMAL(newfrag->node->nref);

	return 0;
}

/* 
 * chfs_remove_frags_of_node -
 * removes all fragments from a fragtree and DOESN'T OBSOLETE them
 */
void
chfs_remove_frags_of_node(struct chfs_mount *chmp, struct rb_tree *fragtree,
	struct chfs_node_ref *nref)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	struct chfs_node_frag *this, *next;

	if (nref == NULL) {
		return;
	}

	/* Iterate the tree and clean all elements. */
	this = (struct chfs_node_frag *)RB_TREE_MIN(fragtree);
	while (this) {
		next = frag_next(fragtree, this);
		if (this->node->nref == nref) {
			rb_tree_remove_node(fragtree, this);
			chfs_free_node_frag(this);
		}
		this = next;
	}
}

/*
 * chfs_kill_fragtree - 
 * removes all fragments from a fragtree and OBSOLETES them
 */
void
chfs_kill_fragtree(struct chfs_mount *chmp, struct rb_tree *fragtree)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	struct chfs_node_frag *this, *next;

	/* Iterate the tree and clean all elements. */
	this = (struct chfs_node_frag *)RB_TREE_MIN(fragtree);
	while (this) {
		next = frag_next(fragtree, this);
		rb_tree_remove_node(fragtree, this);
		chfs_obsolete_node_frag(chmp, this);
		this = next;
	}
}

/* chfs_truncate_fragtree - truncates the tree to a specified size */
uint32_t
chfs_truncate_fragtree(struct chfs_mount *chmp,
	struct rb_tree *fragtree, uint32_t size)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	struct chfs_node_frag *frag;

	dbg("truncate to size: %u\n", size);

	frag = (struct chfs_node_frag *)rb_tree_find_node_leq(fragtree, &size);

	/* Find the last frag before size and set its new size. */
	if (frag && frag->ofs != size) {
		if (frag->ofs + frag->size > size) {
			frag->size = size - frag->ofs;
		}
		frag = frag_next(fragtree, frag);
	}

	/* Delete frags after new size. */
	while (frag && frag->ofs >= size) {
		struct chfs_node_frag *next = frag_next(fragtree, frag);

		rb_tree_remove_node(fragtree, frag);
		chfs_obsolete_node_frag(chmp, frag);
		frag = next;
	}

	if (size == 0) {
		return 0;
	}

	frag = frag_last(fragtree);

	if (!frag) {
		return 0;
	}
	
	if (frag->ofs + frag->size < size) {
		return frag->ofs + frag->size;
	}

	/* FIXME Should we check the postion of the last node? (PAGE_CACHE size, etc.) */
	if (frag->node && (frag->ofs & (PAGE_SIZE - 1)) == 0) {
		frag->node->nref->nref_offset =
			CHFS_GET_OFS(frag->node->nref->nref_offset) | CHFS_PRISTINE_NODE_MASK;
	}

	return size;
}

/* chfs_obsolete_node_frag - obsoletes a fragment of a node */
void
chfs_obsolete_node_frag(struct chfs_mount *chmp,
    struct chfs_node_frag *this)
{
	struct chfs_vnode_cache *vc;
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	if (this->node) {
	/* The fragment is in a node. */
		KASSERT(this->node->frags != 0);
		this->node->frags--;
		if (this->node->frags == 0) {
		/* This is the last fragment. (There is no more.) */
			KASSERT(!CHFS_REF_OBSOLETE(this->node->nref));
			mutex_enter(&chmp->chm_lock_vnocache);
			vc = chfs_nref_to_vc(this->node->nref);
			dbg("[MARK] lnr: %u ofs: %u\n", this->node->nref->nref_lnr, 
				this->node->nref->nref_offset);

			chfs_remove_and_obsolete(chmp, vc, this->node->nref, &vc->dnode);	
			mutex_exit(&chmp->chm_lock_vnocache);

			chfs_free_full_dnode(this->node);
		} else {
		/* There is more frags in the node. */
			CHFS_MARK_REF_NORMAL(this->node->nref);
		}
	}
	chfs_free_node_frag(this);
}

/* chfs_add_full_dnode_to_inode - adds a data node to an inode */
int
chfs_add_full_dnode_to_inode(struct chfs_mount *chmp,
    struct chfs_inode *ip,
    struct chfs_full_dnode *fd)
{
	int ret;
	struct chfs_node_frag *newfrag;
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	if (unlikely(!fd->size))
		return 0;

	/* Create a new fragment from the data node and add it to the fragtree. */
	newfrag = new_fragment(fd, fd->ofs, fd->size);
	if (unlikely(!newfrag))
		return ENOMEM;

	ret = chfs_add_frag_to_fragtree(chmp, &ip->fragtree, newfrag);
	if (ret)
		return ret;

	/* Check previous fragment. */
	if (newfrag->ofs & (PAGE_SIZE - 1)) {
		struct chfs_node_frag *prev = frag_prev(&ip->fragtree, newfrag);

		CHFS_MARK_REF_NORMAL(fd->nref);
		if (prev->node)
			CHFS_MARK_REF_NORMAL(prev->node->nref);
	}

	/* Check next fragment. */
	if ((newfrag->ofs+newfrag->size) & (PAGE_SIZE - 1)) {
		struct chfs_node_frag *next = frag_next(&ip->fragtree, newfrag);

		if (next) {
			CHFS_MARK_REF_NORMAL(fd->nref);
			if (next->node)
				CHFS_MARK_REF_NORMAL(next->node->nref);
		}
	}

	return 0;
}


/* chfs_get_data_nodes - get temporary nodes of an inode */
int
chfs_get_data_nodes(struct chfs_mount *chmp,
    struct chfs_inode *ip,
    struct chfs_readinode_info *rii)
{
	uint32_t crc;
	int err;
	size_t len, retlen;
	struct chfs_node_ref *nref;
	struct chfs_flash_data_node *dnode;
	struct chfs_tmp_dnode *td;
	char* buf;

	len = sizeof(struct chfs_flash_data_node);
	buf = kmem_alloc(len, KM_SLEEP);

	dnode = kmem_alloc(len, KM_SLEEP);
	if (!dnode) {
		kmem_free(buf, len);
		return ENOMEM;
	}

	nref = chfs_first_valid_data_ref(ip->chvc->dnode);

	/* Update highest version. */
	rii->highest_version = ip->chvc->highest_version;

	while(nref && (struct chfs_vnode_cache *)nref != ip->chvc) {
		err = chfs_read_leb(chmp, nref->nref_lnr, buf, CHFS_GET_OFS(nref->nref_offset), len, &retlen);
		if (err || len != retlen)
			goto out;
		dnode = (struct chfs_flash_data_node*)buf;

		/* Check header crc. */
		crc = crc32(0, (uint8_t *)dnode, CHFS_NODE_HDR_SIZE - 4);
		if (crc != le32toh(dnode->hdr_crc)) {
			chfs_err("CRC check failed. calc: 0x%x orig: 0x%x\n", crc, le32toh(dnode->hdr_crc));
			goto cont;
		}

		/* Check header magic bitmask. */
		if (le16toh(dnode->magic) != CHFS_FS_MAGIC_BITMASK) {
			chfs_err("Wrong magic bitmask.\n");
			goto cont;
		}

		/* Check node crc. */
		crc = crc32(0, (uint8_t *)dnode, sizeof(*dnode) - 4);
		if (crc != le32toh(dnode->node_crc)) {
			chfs_err("Node CRC check failed. calc: 0x%x orig: 0x%x\n", crc, le32toh(dnode->node_crc));
			goto cont;
		}

		td = chfs_alloc_tmp_dnode();
		if (!td) {
			chfs_err("Can't allocate tmp dnode info.\n");
			err = ENOMEM;
			goto out;
		}

		/* We don't check data crc here, just add nodes to tmp frag tree, because
		 * we don't want to check nodes which have been overlapped by a new node
		 * with a higher version number.
		 */
		td->node = chfs_alloc_full_dnode();
		if (!td->node) {
			chfs_err("Can't allocate full dnode info.\n");
			err = ENOMEM;
			goto out_tmp_dnode;
		}
		td->version = le64toh(dnode->version);
		td->node->ofs = le64toh(dnode->offset);
		td->data_crc = le32toh(dnode->data_crc);
		td->node->nref = nref;
		td->node->size = le32toh(dnode->data_length);
		td->node->frags = 1;
		td->overlapped = 0;

		if (td->version > rii->highest_version) {
			rii->highest_version = td->version;
		}

		/* Add node to the tree. */
		err = chfs_add_tmp_dnode_to_tree(chmp, rii, td);
		if (err)
			goto out_full_dnode;

cont:
		nref = chfs_first_valid_data_ref(nref->nref_next);
	}

	ip->chvc->highest_version = rii->highest_version;
	return 0;

out_full_dnode:
	chfs_free_full_dnode(td->node);
out_tmp_dnode:
	chfs_free_tmp_dnode(td);
out:
	kmem_free(buf, len);
	kmem_free(dnode, len);
	return err;
}


/* chfs_build_fragtree - builds fragtree from temporary tree */
int
chfs_build_fragtree(struct chfs_mount *chmp, struct chfs_inode *ip,
    struct chfs_readinode_info *rii)
{
	struct chfs_tmp_dnode_info *pen, *last, *this;
	struct rb_tree ver_tree;    /* version tree, used only temporary */
	uint64_t high_ver = 0;
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	rb_tree_init(&ver_tree, &tmp_node_rbtree_ops);

	/* Update highest version and latest node reference. */
	if (rii->mdata_tn) {
		high_ver = rii->mdata_tn->tmpnode->version;
		rii->latest_ref = rii->mdata_tn->tmpnode->node->nref;
	}

	/* Iterate the temporary tree in reverse order. */
	pen = (struct chfs_tmp_dnode_info *)RB_TREE_MAX(&rii->tdi_root);

	while((last = pen)) {
		pen = (struct chfs_tmp_dnode_info *)rb_tree_iterate(&rii->tdi_root, last, RB_DIR_LEFT);

		/* We build here a version tree from overlapped nodes. */
		rb_tree_remove_node(&rii->tdi_root, last);
		rb_tree_insert_node(&ver_tree, last);

		if (last->tmpnode->overlapped) {
			if (pen)
				continue;

			last->tmpnode->overlapped = 0;
		}
		
		this = (struct chfs_tmp_dnode_info *)RB_TREE_MAX(&ver_tree);

		/* Start to build the fragtree. */
		while (this) {
			struct chfs_tmp_dnode_info *vers_next;
			int ret;

			vers_next = (struct chfs_tmp_dnode_info *)rb_tree_iterate(&ver_tree, this, RB_DIR_LEFT);
			rb_tree_remove_node(&ver_tree, this);

			struct chfs_tmp_dnode *tmp_td = this->tmpnode;
			while (tmp_td) {
				struct chfs_tmp_dnode *next_td = tmp_td->next;
				
				/* Check temporary node. */
				if (chfs_check_td_node(chmp, tmp_td)) {
					if (next_td) {
						chfs_remove_tmp_dnode_from_tdi(this, tmp_td);
						chfs_kill_td(chmp, tmp_td);
					} else {
						break;
					}
				} else {
					if (tmp_td->version > high_ver) {
						high_ver = tmp_td->version;
						dbg("highver: %llu\n", (unsigned long long)high_ver);
						rii->latest_ref = tmp_td->node->nref;
					}

					/* Add node to inode and its fragtree. */
					ret = chfs_add_full_dnode_to_inode(chmp, ip, tmp_td->node);
					if (ret) {
						/* On error, clean the whole version tree. */
						while (1) {
							vers_next = (struct chfs_tmp_dnode_info *)rb_tree_iterate(&ver_tree, this, RB_DIR_LEFT);
							while (tmp_td) {
								next_td = tmp_td->next;

								chfs_free_full_dnode(tmp_td->node);
								chfs_remove_tmp_dnode_from_tdi(this, tmp_td);
								chfs_kill_td(chmp, tmp_td);
								tmp_td = next_td;
							}
							chfs_free_tmp_dnode_info(this);
							this = vers_next;
							if (!this)
								break;
							rb_tree_remove_node(&ver_tree, vers_next);
							chfs_kill_tdi(chmp, vers_next);
						}
						return ret;
					}

					/* Remove temporary node from temporary descriptor.
					 * Shouldn't obsolete tmp_td here, because tmp_td->node
					 * was added to the inode. */
					chfs_remove_tmp_dnode_from_tdi(this, tmp_td);
					chfs_free_tmp_dnode(tmp_td);
				}
				tmp_td = next_td;
			}
			/* Continue with the previous element of version tree. */
			chfs_kill_tdi(chmp, this);
			this = vers_next;
		}
	}

	return 0;
}

/* chfs_read_inode - checks the state of the inode then reads and builds it */
int chfs_read_inode(struct chfs_mount *chmp, struct chfs_inode *ip)
{
	struct chfs_vnode_cache *vc = ip->chvc;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

retry:
	mutex_enter(&chmp->chm_lock_vnocache);
	switch (vc->state) {
		case VNO_STATE_UNCHECKED:
			/* FALLTHROUGH */
		case VNO_STATE_CHECKEDABSENT:
			vc->state = VNO_STATE_READING;
			break;
		case VNO_STATE_CHECKING:
			/* FALLTHROUGH */
		case VNO_STATE_GC:
			mutex_exit(&chmp->chm_lock_vnocache);
			goto retry;
			break;
		case VNO_STATE_PRESENT:
			/* FALLTHROUGH */
		case VNO_STATE_READING:
			chfs_err("Reading inode #%llu in state %d!\n",
				(unsigned long long)vc->vno, vc->state);
			chfs_err("wants to read a nonexistent ino %llu\n",
				(unsigned long long)vc->vno);
			return ENOENT;
		default:
			panic("BUG() Bad vno cache state.");
	}
	mutex_exit(&chmp->chm_lock_vnocache);

	return chfs_read_inode_internal(chmp, ip);
}

/*
 * chfs_read_inode_internal - reads and builds an inode
 * Firstly get temporary nodes then build fragtree.
 */
int
chfs_read_inode_internal(struct chfs_mount *chmp, struct chfs_inode *ip)
{
	int err;
	size_t len, retlen;
	char* buf;
	struct chfs_readinode_info rii;
	struct chfs_flash_vnode *fvnode;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	len = sizeof(*fvnode);

	memset(&rii, 0, sizeof(rii));

	rb_tree_init(&rii.tdi_root, &tmp_node_rbtree_ops);

	/* Build a temporary node tree. */
	err = chfs_get_data_nodes(chmp, ip, &rii);
	if (err) {
		if (ip->chvc->state == VNO_STATE_READING)
			ip->chvc->state = VNO_STATE_CHECKEDABSENT;
		/* FIXME Should we kill fragtree or something here? */
		return err;
	}

	/* Build fragtree from temp nodes. */
	rb_tree_init(&ip->fragtree, &frag_rbtree_ops);

	err = chfs_build_fragtree(chmp, ip, &rii);
	if (err) {
		if (ip->chvc->state == VNO_STATE_READING)
			ip->chvc->state = VNO_STATE_CHECKEDABSENT;
		/* FIXME Should we kill fragtree or something here? */
		return err;
	}

	if (!rii.latest_ref) {
		return 0;
	}

	buf = kmem_alloc(len, KM_SLEEP);
	if (!buf)
		return ENOMEM;

	/* Set inode size from its vnode information node. */
	err = chfs_read_leb(chmp, ip->chvc->v->nref_lnr, buf, CHFS_GET_OFS(ip->chvc->v->nref_offset), len, &retlen);
	if (err || retlen != len) {
		kmem_free(buf, len);
		return err?err:EIO;
	}

	fvnode = (struct chfs_flash_vnode*)buf;

	dbg("set size from v: %u\n", fvnode->dn_size);
	chfs_set_vnode_size(ITOV(ip), fvnode->dn_size);
	uint32_t retsize = chfs_truncate_fragtree(chmp, &ip->fragtree, fvnode->dn_size);
	if (retsize != fvnode->dn_size) {
		dbg("Truncating failed. It is %u instead of %u\n", retsize, fvnode->dn_size);
	}

	kmem_free(buf, len);

	if (ip->chvc->state == VNO_STATE_READING) {
		ip->chvc->state = VNO_STATE_PRESENT;
	}

	return 0;
}

/* chfs_read_data - reads and checks data of a file */
int
chfs_read_data(struct chfs_mount* chmp, struct vnode *vp,
    struct buf *bp)
{
	off_t ofs;
	struct chfs_node_frag *frag;
	char * buf;
	int err = 0;
	size_t size, retlen;
	uint32_t crc;
	struct chfs_inode *ip = VTOI(vp);
	struct chfs_flash_data_node *dnode;
	struct chfs_node_ref *nref;

	memset(bp->b_data, 0, bp->b_bcount);

	/* Calculate the size of the file from its fragtree. */
	ofs = bp->b_blkno * PAGE_SIZE;
	frag = (struct chfs_node_frag *)rb_tree_find_node_leq(&ip->fragtree, &ofs);

	if (!frag || frag->ofs > ofs || frag->ofs + frag->size <= ofs) {
		bp->b_resid = 0;
		dbg("not found in frag tree\n");
		return 0;
	}

	if (!frag->node) {
		dbg("no node in frag\n");
		return 0;
	}

	nref = frag->node->nref;
	size = sizeof(*dnode) + frag->size;

	buf = kmem_alloc(size, KM_SLEEP);

	/* Read node from flash. */
	dbg("reading from lnr: %u, offset: %u, size: %zu\n", nref->nref_lnr, CHFS_GET_OFS(nref->nref_offset), size);
	err = chfs_read_leb(chmp, nref->nref_lnr, buf, CHFS_GET_OFS(nref->nref_offset), size, &retlen);
	if (err) {
		chfs_err("error after reading: %d\n", err);
		goto out;
	}
	if (retlen != size) {
		chfs_err("retlen: %zu != size: %zu\n", retlen, size);
		err = EIO;
		goto out;
	}

	/* Read data from flash. */
	dnode = (struct chfs_flash_data_node *)buf;
	crc = crc32(0, (uint8_t *)dnode, CHFS_NODE_HDR_SIZE - 4);
	if (crc != le32toh(dnode->hdr_crc)) {
		chfs_err("CRC check failed. calc: 0x%x orig: 0x%x\n", crc, le32toh(dnode->hdr_crc));
		err = EIO;
		goto out;
	}

	/* Check header magic bitmask. */
	if (le16toh(dnode->magic) != CHFS_FS_MAGIC_BITMASK) {
		chfs_err("Wrong magic bitmask.\n");
		err = EIO;
		goto out;
	}

	/* Check crc of node. */
	crc = crc32(0, (uint8_t *)dnode, sizeof(*dnode) - 4);
	if (crc != le32toh(dnode->node_crc)) {
		chfs_err("Node CRC check failed. calc: 0x%x orig: 0x%x\n", crc, le32toh(dnode->node_crc));
		err = EIO;
		goto out;
	}

	/* Check crc of data. */
	crc = crc32(0, (uint8_t *)dnode->data, dnode->data_length);
	if (crc != le32toh(dnode->data_crc)) {
		chfs_err("Data CRC check failed. calc: 0x%x orig: 0x%x\n", crc, le32toh(dnode->data_crc));
		err = EIO;
		goto out;
	}

	memcpy(bp->b_data, dnode->data, dnode->data_length);
	bp->b_resid = 0;

out:
	kmem_free(buf, size);
	return err;
}

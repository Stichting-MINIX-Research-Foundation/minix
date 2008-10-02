/*
read_nlist.c

Read the symbol table of an executable into memory. 

Created:	Mar 6, 1992 by Philip Homburg
*/

#include <sys/types.h>
#include <assert.h>
#include <a.out.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extra.h"

#define USE_OLD_NLIST	1

static u16_t le16 _ARGS(( u16_t *word_p ));
static u32_t le32 _ARGS(( u32_t *word_p ));
static u16_t be16 _ARGS(( u16_t *word_p ));
static u32_t be32 _ARGS(( u32_t *word_p ));

static u16_t le16(word_p)
u16_t *word_p;
{
	return (((u8_t *)word_p)[0] << 0) | (((u8_t *)word_p)[1] << 8);
}

static u16_t be16(word_p)
u16_t *word_p;
{
	return (((u8_t *)word_p)[1] << 0) | (((u8_t *)word_p)[0] << 8);
}

static u32_t le32(dword_p)
u32_t *dword_p;
{
	return le16(&((u16_t *)dword_p)[0]) |
		((u32_t)le16(&((u16_t *)dword_p)[1]) << 16);
}

static u32_t be32(dword_p)
u32_t *dword_p;
{
	return be16(&((u16_t *)dword_p)[1]) |
		((u32_t)be16(&((u16_t *)dword_p)[0]) << 16);
}

#ifndef USE_OLD_NLIST
struct old_nlist
{
	char n_name[8];
	long n_value;
	unsigned char n_sclass;
	unsigned char n_numaux;
	unsigned char n_type;
};

/* Low bits of storage class (section). */
#define O_N_SECT            07    /* section mask */
#define O_N_UNDF            00    /* undefined */
#define O_N_ABS             01    /* absolute */
#define O_N_TEXT            02    /* text */
#define O_N_DATA            03    /* data */
#define O_N_BSS             04    /* bss */
#define O_N_COMM            05    /* (common) */

/* High bits of storage class. */
#define O_N_CLASS         0370    /* storage class mask */
#define O_C_NULL
#define O_C_EXT           0020    /* external symbol */
#define O_C_STAT          0030    /* static */
#endif

int read_nlist(filename, nlist_table)
const char *filename;	
struct nlist **nlist_table;
{
	int r, n_entries, save_err;
	u32_t (*cnv32) _ARGS(( u32_t *addr ));
	u16_t (*cnv16) _ARGS(( u16_t *addr ));
	struct nlist *nlist_p, *nlist_ptr;
	int exec_fd;
	struct exec exec_header;
#ifndef USE_OLD_NLIST
	struct old_nlist *old_nlist, *old_nlist_p;
	void *old_nlist_end;
#endif
	char *str_tab, *str_base, *str_null;
	u32_t string_siz;

	exec_fd= open(filename, O_RDONLY);
	if (exec_fd == -1)			/* No executable present */
	{
		return -1;
	}

	r= read(exec_fd, (char *)&exec_header, A_MINHDR);
	if (r != A_MINHDR)
	{
		if (r != -1)
			errno= ENOEXEC;
		return -1;
	}
	if (BADMAG(exec_header))
	{
		errno= ENOEXEC;
		return -1;
	}
	switch(exec_header.a_cpu & 3)
	{
	case 0:	/* little endian */
		cnv32= le32;
		cnv16= le16;
		break;
	case 3:	/* big endian */
		cnv32= be32;
		cnv16= be16;
		break;
	default:
		errno= ENOEXEC;
		return -1;
	}
	exec_header.a_version= cnv16((u16_t *)&exec_header.a_version);
	exec_header.a_text= cnv32((u32_t *)&exec_header.a_text);
	exec_header.a_data= cnv32((u32_t *)&exec_header.a_data);
	exec_header.a_bss= cnv32((u32_t *)&exec_header.a_bss);
	exec_header.a_entry= cnv32((u32_t *)&exec_header.a_entry);
	exec_header.a_total= cnv32((u32_t *)&exec_header.a_total);
	exec_header.a_syms= cnv32((u32_t *)&exec_header.a_syms);
	exec_header.a_trsize= cnv32((u32_t *)&exec_header.a_trsize);
	exec_header.a_drsize= cnv32((u32_t *)&exec_header.a_drsize);
	exec_header.a_tbase= cnv32((u32_t *)&exec_header.a_tbase);
	exec_header.a_dbase= cnv32((u32_t *)&exec_header.a_dbase);

	if (!exec_header.a_syms)
		return 0;
		
	if (exec_header.a_flags & A_NSYM)
	{
#if USE_OLD_NLIST
		errno= EINVAL;
		return -1;
#else
		r= lseek(exec_fd, A_SYMPOS(exec_header)+exec_header.a_syms,
			SEEK_SET);
		if (r == -1)
		{
			return -1;
		}
		r= read(exec_fd, (char *)&string_siz, 4);
		if (r != 4)
			return -1;
		string_siz= cnv32(&string_siz)-4;
		nlist_p= malloc(exec_header.a_syms + string_siz+1);
		if (!nlist_p)
		{
			errno= ENOMEM;
			return -1;
		}
		r= lseek(exec_fd, A_SYMPOS(exec_header)+exec_header.a_syms+4,
			SEEK_SET);
		if (r == -1)
		{
			save_err= errno;
			free(nlist_p);
			errno= save_err;
			return -1;
		}
		r= read(exec_fd, ((char *)nlist_p)+exec_header.a_syms,
			string_siz);
		if (r != string_siz)
		{
			save_err= errno;
			free(nlist_p);
			errno= save_err;
			return -1;
		}
		r= lseek(exec_fd, A_SYMPOS(exec_header), SEEK_SET);
		if (r == -1)
		{
			save_err= errno;
			free(nlist_p);
			errno= save_err;
			return -1;
		}
		r= read(exec_fd, ((char *)nlist_p), exec_header.a_syms);
		if (r != exec_header.a_syms)
		{
			save_err= errno;
			free(nlist_p);
			errno= save_err;
			return -1;
		}
		str_base= ((char *)nlist_p) + exec_header.a_syms -4;
		str_null= (char *)nlist_p + exec_header.a_syms + string_siz;
		*str_null= '\0';
		for (nlist_ptr= nlist_p; (char *)nlist_ptr+1 <= str_base+4;
								nlist_ptr++)
		{
			nlist_ptr->n_desc= le16((u16_t *)&nlist_ptr->n_desc);
			nlist_ptr->n_value= le32(&nlist_ptr->n_value);
			if (nlist_ptr->n_un.n_strx)
				nlist_ptr->n_un.n_name= str_base+
					cnv32((u32_t *)&nlist_ptr->n_un.n_strx);
			else
				nlist_ptr->n_un.n_name= str_null;
		}
		*nlist_table= nlist_p;
		return nlist_ptr-nlist_p;
#endif
	}
	else
	{
		r= lseek(exec_fd, A_SYMPOS(exec_header), SEEK_SET);
		if (r == -1)
		{
			return -1;
		}
		
#if USE_OLD_NLIST
		n_entries= exec_header.a_syms/sizeof(struct nlist);
		nlist_p= malloc(exec_header.a_syms);
		if (!nlist_p)
		{
			free(nlist_p);
			errno= ENOMEM;
			return -1;
		}
		r= read(exec_fd, (char *)nlist_p, exec_header.a_syms);
		if (r != exec_header.a_syms)
		{
			save_err= errno;
			free(nlist_p);
			errno= save_err;
			return -1;
		}

		*nlist_table= nlist_p;
		return n_entries;
#else
		n_entries= exec_header.a_syms/sizeof(struct old_nlist)+1;
		nlist_p= NULL;
		old_nlist= NULL;

		nlist_p= malloc(n_entries * (sizeof(struct nlist)+
			sizeof(old_nlist->n_name)+1));
		old_nlist= malloc(exec_header.a_syms);
		if (!old_nlist || !nlist_p)
		{
			if (nlist_p)
				free(nlist_p);
			if (old_nlist)
				free(old_nlist);
			errno= ENOMEM;
			return -1;
		}
		r= read(exec_fd, (char *)old_nlist, exec_header.a_syms);
		if (r != exec_header.a_syms)
		{
			save_err= errno;
			free(nlist_p);
			free(old_nlist);
			errno= save_err;
			return -1;
		}

		old_nlist_end= (char *)old_nlist+exec_header.a_syms;
		str_tab= (char *)&nlist_p[n_entries];
		n_entries= 0;
		for (old_nlist_p= old_nlist; 
			(void *)(old_nlist_p+1)<=old_nlist_end; old_nlist_p++)
		{
			switch(old_nlist_p->n_sclass & O_N_SECT)
			{
			case O_N_UNDF:
				nlist_p[n_entries].n_type= N_UNDF;
				break;
			case O_N_ABS:
				nlist_p[n_entries].n_type= N_ABS;
				break;
			case O_N_TEXT:
				nlist_p[n_entries].n_type= N_TEXT;
				break;
			case O_N_DATA:
				nlist_p[n_entries].n_type= N_DATA;
				break;
			case O_N_BSS:
				nlist_p[n_entries].n_type= N_BSS;
				break;
			case O_N_COMM:
				nlist_p[n_entries].n_type= N_COMM;
				break;
			default:
				continue;
			}
			switch(old_nlist_p->n_sclass & O_N_CLASS)
			{
			case O_C_EXT:
				nlist_p[n_entries].n_type |= N_EXT;
			case O_C_STAT:
				break;
			default:
				continue;
			}
			nlist_p[n_entries].n_value= 
				cnv32((u32_t *)&old_nlist_p->n_value);
			nlist_p[n_entries].n_un.n_name= str_tab;
			memcpy(str_tab, old_nlist_p->n_name, 
				sizeof(old_nlist_p->n_name));
			str_tab += sizeof(old_nlist_p->n_name);
			*str_tab++= '\0';
			n_entries++;
		}
		free(old_nlist);
		nlist_p= realloc(nlist_p, str_tab-(char *)nlist_p);
		*nlist_table= nlist_p;
		return n_entries;
#endif
	}
}

/*
 * $PchId: read_nlist.c,v 1.5 1996/04/11 07:47:38 philip Exp $
 */

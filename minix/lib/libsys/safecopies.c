
/* Library functions to maintain internal data copying tables.
 *
 * April 21 2006: Initial version (Ben Gras)
 *
 */

#include <lib.h>
#include <errno.h>
#include <minix/sysutil.h>
#include <assert.h>
#include <stdlib.h>
#include <minix/syslib.h>
#include <minix/safecopies.h>
#include <minix/com.h>
#include <string.h>

#define ACCESS_CHECK(a) { 			\
	if((a) & ~(CPF_READ|CPF_WRITE|CPF_TRY)) {	\
		errno = EINVAL;			\
		return -1;			\
	}					\
   }

#define GID_CHECK(gid) {					\
	if(!GRANT_VALID(gid) || GRANT_IDX(gid) >= ngrants ||	\
	    GRANT_SEQ(gid) != grants[GRANT_IDX(gid)].cp_seq) {	\
		errno = EINVAL;					\
		return -1;					\
	}							\
   }

#define GID_CHECK_USED(gid) {					\
	GID_CHECK(gid);						\
	if(!(grants[GRANT_IDX(gid)].cp_flags & CPF_USED)) {	\
		errno = EINVAL;					\
		return -1;					\
	}							\
   }

#define NR_STATIC_GRANTS 3
static cp_grant_t static_grants[NR_STATIC_GRANTS];
static cp_grant_t *grants = NULL;
static int ngrants = 0;
static int freelist = -1;

/*
 * Preallocate more grants that will be free for subsequent use.  If a specific
 * number of grants is given (i.e., count > 0), the total number of grants will
 * be increased by that amount.  If no number of grants is given (count == 0),
 * double(ish) the size of the table.  The latter is used internally.  This
 * function may fail, either because the maximum number of slots is reached or
 * because no new memory can be allocated.  In that case, nothing will change;
 * the caller must check afterward whether there are newly available grants.
 */
void
cpf_prealloc(unsigned int count)
{
	cp_grant_t *new_grants;
	int g, new_size;

	if (!ngrants && count <= NR_STATIC_GRANTS) {
		/* Use statically allocated grants the first time. */
		new_size = NR_STATIC_GRANTS;
		new_grants = static_grants;
	}
	else {
		if (ngrants >= GRANT_MAX_IDX)
			return;
		if (count != 0) {
			if (count > (unsigned)(GRANT_MAX_IDX - ngrants))
				count = (unsigned)(GRANT_MAX_IDX - ngrants);
			new_size = ngrants + (int)count;
		} else
			new_size = (1+ngrants)*2;
		if (new_size >= GRANT_MAX_IDX)
			new_size = GRANT_MAX_IDX;
		assert(new_size > ngrants);

		/* Allocate a block of new size. */
		if(!(new_grants=malloc(new_size * sizeof(grants[0])))) {
			return;
		}
	}

	/* Copy old block to new block. */
	if(grants && ngrants > 0)
		memcpy(new_grants, grants, ngrants * sizeof(grants[0]));

	/*
	 * Make sure new slots are marked unused (CPF_USED is clear).
	 * Also start with a zero sequence number, for consistency; since the
	 * grant table is never shrunk, this introduces no issues by itself.
	 * Finally, form a new free list, in ascending order so that the lowest
	 * IDs get allocated first.  Both the zeroed sequence number and the
	 * ascending order are necessary so that the first grant to be
	 * allocated has a zero ID (see the live update comment below).
	 */
	for(g = ngrants; g < new_size; g++) {
		new_grants[g].cp_flags = 0;
		new_grants[g].cp_seq = 0;
		new_grants[g].cp_u.cp_free.cp_next =
		    (g < new_size - 1) ? (g + 1) : freelist;
	}

	/* Inform kernel about new size (and possibly new location). */
	if((sys_setgrant(new_grants, new_size))) {
                if(new_grants != static_grants) free(new_grants);
		return;	/* Failed - don't grow then. */
	}

	/* Update internal data. */
	if(grants && ngrants > 0 && grants != static_grants) free(grants);
	freelist = ngrants;
	grants = new_grants;
	ngrants = new_size;
}

static int
cpf_new_grantslot(void)
{
/* Find a new, free grant slot in the grant table, grow it if
 * necessary. If no free slot is found and the grow failed,
 * return -1. Otherwise, return grant slot number.
 */
	int g;

	/* Obtain a free slot. */
	if ((g = freelist) == -1) {
		/* Table full - try to make the table larger. */
		cpf_prealloc(0);
		if ((g = freelist) == -1) {
			/* ngrants hasn't increased. */
			errno = ENOSPC;
			return -1;
		}
	}

	/* Basic sanity checks - if we get this far, g must be a valid,
	 * free slot.
	 */
	assert(g >= 0);
	assert(g < ngrants);
	assert(!(grants[g].cp_flags & CPF_USED));

	/* Take the slot off the free list, and return its slot number. */
	freelist = grants[g].cp_u.cp_free.cp_next;

	return g;
}

cp_grant_id_t
cpf_grant_direct(endpoint_t who_to, vir_bytes addr, size_t bytes, int access)
{
	int g;
 
	ACCESS_CHECK(access);

	/* Get new slot to put new grant in. */
	if((g = cpf_new_grantslot()) < 0)
		return -1;

	/* Fill in new slot data. */
	grants[g].cp_u.cp_direct.cp_who_to = who_to;
	grants[g].cp_u.cp_direct.cp_start = addr;
	grants[g].cp_u.cp_direct.cp_len = bytes;
	grants[g].cp_faulted = GRANT_INVALID;
	__insn_barrier();
	grants[g].cp_flags = access | CPF_DIRECT | CPF_USED | CPF_VALID;

	return GRANT_ID(g, grants[g].cp_seq);
}

cp_grant_id_t
cpf_grant_indirect(endpoint_t who_to, endpoint_t who_from, cp_grant_id_t gr)
{
/* Grant process A access into process B. B has granted us access as grant
 * id 'gr'.
 */
	int g;

	/* Obtain new slot. */
	if((g = cpf_new_grantslot()) < 0)
		return -1;

	/* Fill in new slot data. */
	grants[g].cp_u.cp_indirect.cp_who_to = who_to;
	grants[g].cp_u.cp_indirect.cp_who_from = who_from;
	grants[g].cp_u.cp_indirect.cp_grant = gr;
	grants[g].cp_faulted = GRANT_INVALID;
	__insn_barrier();
	grants[g].cp_flags = CPF_USED | CPF_INDIRECT | CPF_VALID;

	return GRANT_ID(g, grants[g].cp_seq);
}

cp_grant_id_t
cpf_grant_magic(endpoint_t who_to, endpoint_t who_from,
	vir_bytes addr, size_t bytes, int access)
{
/* Grant process A access into process B. Not everyone can do this. */
	int g;

	ACCESS_CHECK(access);

	/* Obtain new slot. */
	if((g = cpf_new_grantslot()) < 0)
		return -1;

	/* Fill in new slot data. */
	grants[g].cp_u.cp_magic.cp_who_to = who_to;
	grants[g].cp_u.cp_magic.cp_who_from = who_from;
	grants[g].cp_u.cp_magic.cp_start = addr;
	grants[g].cp_u.cp_magic.cp_len = bytes;
	grants[g].cp_faulted = GRANT_INVALID;
	__insn_barrier();
	grants[g].cp_flags = CPF_USED | CPF_MAGIC | CPF_VALID | access;

	return GRANT_ID(g, grants[g].cp_seq);
}

/*
 * Revoke previously granted access, identified by grant ID.  Return -1 on
 * error, with errno set as appropriate.  Return 0 on success, with one
 * exception: return GRANT_FAULTED (1) if a grant was created with CPF_TRY and
 * during its lifetime, a copy from or to the grant experienced a soft fault.
 */
int
cpf_revoke(cp_grant_id_t grant)
{
	int r, g;

	GID_CHECK_USED(grant);

	g = GRANT_IDX(grant);

	/*
	 * If a safecopy action on a (direct or magic) grant with the CPF_TRY
	 * flag failed on a soft fault, the kernel will have set the cp_faulted
	 * field to the grant identifier.  Here, we test this and return
	 * GRANT_FAULTED (1) on a match.
	 */
	r = ((grants[g].cp_flags & CPF_TRY) &&
	    grants[g].cp_faulted == grant) ? GRANT_FAULTED : 0;

	/*
	 * Make grant invalid by setting flags to 0, clearing CPF_USED.
	 * This invalidates the grant.
	 */
	grants[g].cp_flags = 0;
	__insn_barrier();

	/*
	 * Increase the grant slot's sequence number now, rather than on
	 * allocation, because live update relies on the first allocated grant
	 * having a zero ID (SEF_STATE_TRANSFER_GID) and thus a zero sequence
	 * number.
	 */
	if (grants[g].cp_seq < GRANT_MAX_SEQ - 1)
		grants[g].cp_seq++;
	else
		grants[g].cp_seq = 0;

	/*
	 * Put the grant back on the free list.  The list is single-headed, so
	 * the last freed grant will be the first to be reused.  Especially
	 * given the presence of sequence numbers, this is not a problem.
	 */
	grants[g].cp_u.cp_free.cp_next = freelist;
	freelist = g;

	return r;
}

/*
 * START OF DEPRECATED API
 *
 * The grant preallocation and (re)assignment API below imposes that grant IDs
 * stay the same across reuse, thus disallowing that the grants' sequence
 * numbers be updated as a part of reassignment.  As a result, this API does
 * not offer the same protection against accidental reuse of an old grant by a
 * remote party as the regular API does, and is therefore deprecated.
 */
int
cpf_getgrants(cp_grant_id_t *grant_ids, int n)
{
	int i;

	for(i = 0; i < n; i++) {
	  if((grant_ids[i] = cpf_new_grantslot()) < 0)
		break;
	  grants[grant_ids[i]].cp_flags = CPF_USED;
	  grants[grant_ids[i]].cp_seq = 0;
	}

	/* return however many grants were assigned. */
	return i;
}

int
cpf_setgrant_direct(gid, who, addr, bytes, access)
cp_grant_id_t gid;
endpoint_t who;
vir_bytes addr;
size_t bytes;
int access;
{
	GID_CHECK(gid);
	ACCESS_CHECK(access);

	/* Fill in new slot data. */
	grants[gid].cp_flags = access | CPF_DIRECT | CPF_USED | CPF_VALID;
	grants[gid].cp_u.cp_direct.cp_who_to = who;
	grants[gid].cp_u.cp_direct.cp_start = addr;
	grants[gid].cp_u.cp_direct.cp_len = bytes;

	return 0;
}

int
cpf_setgrant_indirect(gid, who_to, who_from, his_gid)
cp_grant_id_t gid;
endpoint_t who_to, who_from;
cp_grant_id_t his_gid;
{
	GID_CHECK(gid);

	/* Fill in new slot data. */
	grants[gid].cp_flags = CPF_USED | CPF_INDIRECT | CPF_VALID;
	grants[gid].cp_u.cp_indirect.cp_who_to = who_to;
	grants[gid].cp_u.cp_indirect.cp_who_from = who_from;
	grants[gid].cp_u.cp_indirect.cp_grant = his_gid;

	return 0;
}

int
cpf_setgrant_magic(gid, who_to, who_from, addr, bytes, access)
cp_grant_id_t gid;
endpoint_t who_to, who_from;
vir_bytes addr;
size_t bytes;
int access;
{
	GID_CHECK(gid);
	ACCESS_CHECK(access);

	/* Fill in new slot data. */
	grants[gid].cp_flags = CPF_USED | CPF_MAGIC | CPF_VALID | access;
	grants[gid].cp_u.cp_magic.cp_who_to = who_to;
	grants[gid].cp_u.cp_magic.cp_who_from = who_from;
	grants[gid].cp_u.cp_magic.cp_start = addr;
	grants[gid].cp_u.cp_magic.cp_len = bytes;

	return 0;
}

int
cpf_setgrant_disable(gid)
cp_grant_id_t gid;
{
	GID_CHECK(gid);

	/* Grant is now no longer valid, but still in use. */
	grants[gid].cp_flags = CPF_USED;

	return 0;
}
/*
 * END OF DEPRECATED API
 */

void
cpf_reload(void)
{
/* Inform the kernel about the location of the grant table. This is needed
 * after a fork.
 */
	if (grants)
		sys_setgrant(grants, ngrants);	/* Do we need error checking? */
}

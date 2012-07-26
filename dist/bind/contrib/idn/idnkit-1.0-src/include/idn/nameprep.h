/* $Id: nameprep.h,v 1.1.1.1 2003-06-04 00:25:39 marka Exp $ */
/*
 * Copyright (c) 2001 Japan Network Information Center.  All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef IDN_NAMEPREP_H
#define IDN_NAMEPREP_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Perform NAMEPREP (mapping, prohibited/unassigned checking).
 */

#include <idn/export.h>
#include <idn/result.h>

/*
 * BIDI type codes.
 */      
typedef enum {
	idn_biditype_r_al,
	idn_biditype_l,
	idn_biditype_others
} idn_biditype_t;

/*
 * A Handle for nameprep operations.
 */
typedef struct idn_nameprep *idn_nameprep_t;


/*
 * The latest version of nameprep.
 */
#define IDN_NAMEPREP_CURRENT	"RFC3491"

/*
 * Create a handle for nameprep operations.
 * The handle is stored in '*handlep', which is used other functions
 * in this module.
 * The version of the NAMEPREP specification can be specified with
 * 'version' parameter.  If 'version' is NULL, the latest version
 * is used.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_notfound		-- specified version not found.
 */
IDN_EXPORT idn_result_t
idn_nameprep_create(const char *version, idn_nameprep_t *handlep);

/*
 * Close a handle, which was created by 'idn_nameprep_create'.
 */
IDN_EXPORT void
idn_nameprep_destroy(idn_nameprep_t handle);

/*
 * Perform character mapping on an UCS4 string specified by 'from', and
 * store the result into 'to', whose length is specified by 'tolen'.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_buffer_overflow	-- result buffer is too small.
 */
IDN_EXPORT idn_result_t
idn_nameprep_map(idn_nameprep_t handle, const unsigned long *from,
		 unsigned long *to, size_t tolen);

/*
 * Check if an UCS4 string 'str' contains any prohibited characters specified
 * by the draft.  If found, the pointer to the first such character is stored
 * into '*found'.  Otherwise '*found' will be NULL.
 *
 * Returns:
 *	idn_success		-- check has been done properly. (But this
 *				   does not mean that no prohibited character
 *				   was found.  Check '*found' to see the
 *				   result.)
 */
IDN_EXPORT idn_result_t
idn_nameprep_isprohibited(idn_nameprep_t handle, const unsigned long *str,
			  const unsigned long **found);

/*
 * Check if an UCS4 string 'str' contains any unassigned characters specified
 * by the draft.  If found, the pointer to the first such character is stored
 * into '*found'.  Otherwise '*found' will be NULL.
 *
 * Returns:
 *	idn_success		-- check has been done properly. (But this
 *				   does not mean that no unassinged character
 *				   was found.  Check '*found' to see the
 *				   result.)
 */
IDN_EXPORT idn_result_t
idn_nameprep_isunassigned(idn_nameprep_t handle, const unsigned long *str,
			  const unsigned long **found);

/*
 * Check if an UCS4 string 'str' is valid string specified by ``bidi check''
 * of the draft.  If it is not valid, the pointer to the first invalid
 * character is stored into '*found'.  Otherwise '*found' will be NULL.
 *
 * Returns:
 *	idn_success		-- check has been done properly. (But this
 *				   does not mean that the string was valid.
 *				   Check '*found' to see the result.)
 */
IDN_EXPORT idn_result_t
idn_nameprep_isvalidbidi(idn_nameprep_t handle, const unsigned long *str,
			 const unsigned long **found);

/*
 * The following functions are for internal use.
 * They are used for this module to be add to the checker and mapper modules.
 */
IDN_EXPORT idn_result_t
idn_nameprep_createproc(const char *parameter, void **handlep);

IDN_EXPORT void
idn_nameprep_destroyproc(void *handle);

IDN_EXPORT idn_result_t
idn_nameprep_mapproc(void *handle, const unsigned long *from,
		     unsigned long *to, size_t tolen);

IDN_EXPORT idn_result_t
idn_nameprep_prohibitproc(void *handle, const unsigned long *str,
			  const unsigned long **found);

IDN_EXPORT idn_result_t
idn_nameprep_unassignedproc(void *handle, const unsigned long *str,
			    const unsigned long **found);

IDN_EXPORT idn_result_t
idn_nameprep_bidiproc(void *handle, const unsigned long *str,
		      const unsigned long **found);

#ifdef __cplusplus
}
#endif

#endif /* IDN_NAMEPREP_H */

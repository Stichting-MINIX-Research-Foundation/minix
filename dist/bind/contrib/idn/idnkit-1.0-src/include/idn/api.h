/* $Id: api.h,v 1.1.1.1 2003-06-04 00:25:35 marka Exp $ */
/*
 * Copyright (c) 2001,2002 Japan Network Information Center.
 * All rights reserved.
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

#ifndef IDN_API_H
#define IDN_API_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <idn/export.h>
#include <idn/result.h>
#include <idn/res.h>

/*
 * Application Programming Interface for Internationalized Domain Name
 * Handling.  This module provides high-level APIs for ordinary applications.
 * Low-level APIs are also available.  See "res.h" for details.
 */

/*
 * Enable or disable IDN conversion scheme.
 *
 * If on_off is 0, IDN conversion scheme is disabled. Otherwise, IDN
 * conversion is enabled even when IDN_DISABLE is defined.
 */
IDN_EXPORT void
idn_enable(int on_off);

/*
 * Set configuration file name.
 * This function is for private use only.
 *
 * When idn_nameinit(1) is called, this module loads `file'.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 *	idn_failure		-- idn_nameinit() has already been
 *				   called.
 */
extern idn_result_t
idn__setconffile(const char *file);

/*
 * Initialize this module, and load configuration from the default
 * configuration file (idn.conf).
 *
 * The initialization will be done only once when this function is
 * called first, while either loading of the configuration file or
 * setting the defaults behavior without the configuration file will
 * be done every time it is called.
 *
 * If load_file is 0, this will set the defaults behavior without the
 * configuration file.  Otherwise, loading of the configuration file
 * occurs.
 *
 * If 'idn_encodename' or 'idn_decodename' is called without calling
 * this function, implicit initialization without the configuration
 * file will be done prior to encoding/decoding process.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nofile		-- cannot open the configuration file.
 *	idn_invalid_syntax	-- syntax error found in the file.
 *	idn_invalid_name	-- there are invalid names (encoding,
 *				   normalization etc.).
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_nameinit(int load_file);

/*
 * Encode internationalized domain name.
 *
 * The encoding process consists of the following 7 steps.
 *
 *    1. Local encoding to UTF-8 conversion
 *       Converts a domain name written with local encoding (e.g. ISO-
 *       8859-1) to UTF-8.
 *    2. Delimiter mapping,
 *       Maps certain characters to period (U+002E, FULL STOP).
 *    3. Local mapping
 *       Apply character mappings according with the TLD of the domain
 *       name.
 *    4. NAMEPREP
 *       Perform NAME preparation described in RFC3491.
 *       This step consists of the following 4 steps:
 *       4.1. Mapping
 *       4.2. Normalization
 *       4.3. Prohibited character check
 *       4.4. Unassigned check
 *    5. ASCII range character check
 *       Checks if the domain name contains non-LDH ASCII character (not
 *       alpha-numeric or hypen), or it begins or end with hypen.
 *    6. UTF-8 to IDN encoding conversion.
 *       Converts the domain name from UTF-8 to ACE (e.g. Punycode).
 *    7. Length check
 *       Checks the length of each label.
 *
 * 'actions' specifies actions and options of the encoding procedure.
 * Its value is a bitwise-or of the following flags:
 *
 *   IDN_LOCALCONV	-- perform local encoding to UTF-8 conversion (step 1)
 *   IDN_DELIMMAP	-- perform delimiter mapping (step 2)
 *   IDN_LOCALMAP	-- perform local mapping (step 3)
 *   IDN_MAP		-- perform mapping (step 4.1)
 *   IDN_NORMALIZE	-- perform normalization (step 4.2)
 *   IDN_PROHCHECK	-- perform prohibited character check (step 4.3)
 *   IDN_UNASCHECK	-- perform unassigned codepoint check (step 4.4)
 *   IDN_ASCCHECK	-- perform ASCII range character check (step 5)
 *   IDN_IDNCONV	-- perform UTF-8 to IDN encoding conversion (step 6)
 *   IDN_LENCHECK	-- perform length check (step 7)
 *
 * Also the following flags are provided for convinience:
 *
 *   IDN_ENCODE_QUERY	-- On libidnkit, perform step 1..7, except for step
 *			   4.4 and 5.
 *			   On libidnkitlite, perform step 2..7, except for
 *			   step 4.4 and 5.
 *   IDN_ENCODE_STORED	-- On libidnkit, perform step 1..7, except for step
 *			   5.
 *			   On libidnkitlite, perform step 2..7, except for
 *			   step 5.
 *   IDN_ENCODE_APP	-- Same as IDN_ENCODE_QUERY.
 *   IDN_NAMEPREP	-- perform NAMEPREP (step 4) without unassigned
 *			   codepoint check (step 4.4).
 *
 * The following flag does not corresponding to a particular action,
 * but an option of conversion process:
 *
 *   IDN_UNDOIFERR	-- If any step fails, the original input name is
 *                         returned.
 *
 * Note that if no flags are specified, 'idn_encodename' does nothing
 * fancy, just copies the given name verbatim.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_invalid_action	-- invalid action flag specified.
 *	idn_invalid_encoding	-- the given string has invalid/illegal
 *				   byte sequence.
 *	idn_invalid_length	-- invalid length of a label.
 *	idn_prohibited		-- prohibited/unassigned code point found.
 *	idn_buffer_overflow	-- 'tolen' is too small.
 *	idn_nomemory		-- malloc failed.
 *
 * Also, if this function is called without calling 'idn_nameinit',
 * the following error codes might be returned.
 *	idn_nofile		-- cannot open the configuration file.
 *	idn_invalid_syntax	-- syntax error found in the file.
 *	idn_invalid_name	-- there are invalid names (encoding,
 *				   normalization etc.).
 */
IDN_EXPORT idn_result_t
idn_encodename(idn_action_t actions, const char *from, char *to, size_t tolen);

/*
 * Decode internationalized domain name.
 *
 * The decoding process consists of the following 5 steps.
 *
 *    1. delimiter mapping
 *       Maps certain characters to period (U+002E, FULL STOP).
 *    2. NAMEPREP
 *       Perform NAME preparation described in RFC3491.
 *       This step consists of the following 4 steps:
 *       2.1. Mapping
 *       2.2. Normalization
 *       2.3. Prohibited character check
 *       2.4. Unassigned check
 *    3. IDN encoding to UTF-8 conversion.
 *       Converts the domain name from ACE (e.g. Punycode) to UCS4.
 *    4. Perform round-trip check.
 *       Encode the result of step 3, and then compare it with the result
 *       of the step 2.  If they are different, the check is failed.
 *    5. Convert UTF-8 to local encoding.
 *       If a character in the domain name cannot be converted to local
 *       encoding, the conversion is failed.
 *
 * 'actions' specifies actions of the decoding procedure.
 * Its value is a bitwise-or of the following flags:
 *
 *   IDN_DELIMMAP	-- perform delimiter mapping (step 1)
 *   IDN_MAP		-- perform mapping (step 2.1)
 *   IDN_NORMALIZE	-- perform normalization (step 2.2)
 *   IDN_PROHCHECK	-- perform prohibited character check (step 2.3)
 *   IDN_UNASCHECK	-- perform unassigned codepoint check (step 2.4)
 *   IDN_IDNCONV	-- perform IDN encoding to UTF-8 conversion (step 3)
 *   IDN_RTCHECK        -- perform round-trip check (step 4)
 *   IDN_ASCCHECK	-- perform ASCII range character check while
 *			   round-trip check (step 4.1)
 *   IDN_LOCALCONV      -- perform UTF-8 to local encoding conversion (step 5)
 *
 * Also the following flags are provided for the convenience:
 *
 *   IDN_DECODE_QUERY	-- On libidnkit, perform step 1..5, except for step
 *			   2.4 and 4.1.
 *			   On libidnkitlite, perform step 1..3, except for
 *			   step 2.4 and 4.1.
 *   IDN_DECODE_STORED	-- On libidnkit, perform step 1..5, except for step
 *			   4.1.
 *			   On libidnkitlite, perform step 1..3, except for
 *			   step 4.1.
 *   IDN_DECODE_APP	-- Same as IDN_DECODE_QUERY.
 *   IDN_NAMEPREP	-- perform NAMEPREP (step 2) without unassigned
 *			   codepoint check (step 2.4).
 *
 * If any step fails, the original input name is returned.
 * 'actions' specifies what actions to take when decoding, and is
 * a bitwise-or of the following flags:
 *
 * Note that if no flags are specified, 'idn_decodename' does nothing
 * but copying the given name verbatim.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_invalid_action	-- invalid action flag specified.
 *	idn_invalid_encoding	-- the given string has invalid/illegal
 *				   byte sequence.
 *	idn_buffer_overflow	-- 'tolen' is too small.
 *	idn_invalid_length	-- length of a label is not 1..63 characters.
 *	idn_nomemory		-- malloc failed.
 *
 * Also, if this function is called without calling 'idn_nameinit',
 * the following error codes might be returned.
 *	idn_nofile		-- cannot open the configuration file.
 *	idn_invalid_syntax	-- syntax error found in the file.
 *	idn_invalid_name	-- there are invalid names (encoding,
 *				   normalization etc.).
 */
IDN_EXPORT idn_result_t
idn_decodename(idn_action_t actions, const char *from, char *to, size_t tolen);

/*
 * Decode internationalized domain name with auxiliary encoding
 * support.
 *
 * This is another API for IDN string decode.  The difference between
 * two is whether the encoding conversion from auxiliary encoding to
 * UTF-8 occurs prior to the actual decode process (read description
 * of idn_res_decodename() above) or not.
 *
 * If auxencoding is NULL, from is treated as UTF-8 encoded string.
 * 
 * Other arguments serve exactly same role as those of
 * idn_res_decodename().
 */
idn_result_t
idn_decodename2(idn_action_t actions, const char *from, char *to, size_t tolen,
		const char *auxencoding);

#ifdef __cplusplus
}
#endif

#endif /* IDN_API_H */

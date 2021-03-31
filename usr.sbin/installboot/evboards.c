/*	$NetBSD: evboards.c,v 1.5 2020/06/07 00:58:58 thorpej Exp $	*/

/*-
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: evboards.c,v 1.5 2020/06/07 00:58:58 thorpej Exp $");
#endif  /* !__lint */

#include <sys/types.h>
#include <sys/param.h>		/* for roundup() */
#include <sys/stat.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef SUPPORT_FDT
#include "libfdt.h"
#endif

#if !HAVE_NBTOOL_CONFIG_H
#include <sys/utsname.h>

#ifdef SUPPORT_OPENFIRMWARE
#include <sys/ioctl.h>
#include <dev/ofw/openfirmio.h>
#endif

#endif /* ! HAVE_NBTOOL_CONFIG_H */

#include "installboot.h"
#include "evboards.h"

/*
 * The board database is implemented as a property list.  The base
 * system provides a set of known boards, keyed by their "compatible"
 * device tree property.
 *
 * The database provided by the base system is meant to help guide
 * the user as to which u-boot package needs to be installed on the
 * system in order to write the boot loader to the boot media.  The
 * base board plist is specific to the $MACHINE (e.g. "evbarm"), and
 * is installed along with the build tools, e.g.:
 *
 * (native location)
 *	/usr/sbin/installboot
 *	/usr/share/installboot/evbarm/boards.plist
 *	/usr/share/installboot/evbmips/boards.plist
 *
 * (example cross host tool location)
 *	/usr/local/xnbsd/bin/nbinstallboot
 *	/usr/local/xnbsd/share/installboot/evbarm/boards.plist
 *	/usr/local/xnbsd/share/installboot/evbmips/boards.plist
 *
 * The schema of the base board plist is as follows:
 *
 * <plist>
 * <dict>
 *	<!--
 *	  -- Key: string matching a "compatible" DT property.
 *	  -- Value: dictionary representing a board object.
 *	  -- (required)
 *	  -->
 *	<key>example,example-board</key>
 *	<dict>
 *		<!--
 *		  -- Key: "description".
 *		  -- Value: string containing the board description.
 *		  -- (required)
 *		  -->
 *		<key>description</key>
 *		<string>Example Co. Example Board</string>
 *
 *		<!--
 *		  -- Key: "u-boot-pkg".
 *		  -- Value: string representing the board-specific
 *		  --        portion of the u-boot package name.
 *		  --        In this example, the package's full name
 *		  --        is "u-boot-exampleboard".  This is used
 *		  --        to recommend to the user which u-boot
 *		  --        package to install.  If not present, then
 *		  --        no package recommendation will be made.
 *		  -- (optional)
 *		  -->
 *		<key>u-boot-pkg</key>
 *		<string>exampleboard</string>
 *	</dict>
 * </dict>
 * </plist>
 *
 * Individual u-boot packages install their own overlay property list
 * files that installboot(8) then scans for.  These overlay files are
 * named "installboot.plist", and are installed alongside the u-boot
 * binaries by the individual u-boot packages, for example:
 *
 *	/usr/pkg/share/u-boot/exampleboard/installboot.plist
 *	/usr/pkg/share/u-boot/exampleboard/u-boot-with-spl.bin
 *
 * installboot(8) scans a set of directories looking for "installboot.plist"
 * overlay files one directory deep.  For example:
 *
 *	/usr/pkg/share/u-boot/
 *				exampleboard/installboot.plist
 *				superarmdeluxe/installboot.plist
 *				dummy/
 *
 * In this example, "/usr/pkg/share/u-boot" is scanned, it would identify
 * "exampleboard" and "superarmdeluxe" as directories containing overlays
 * and load them.
 *
 * The default path scanned for u-boot packages is:
 *
 *	/usr/pkg/share/u-boot
 *
 * This can be overridden with the INSTALLBOOT_UBOOT_PATHS environment
 * variable, which contains a colon-sparated list of directories, e.g.:
 *
 *	/usr/pkg/share/u-boot:/home/jmcneill/hackityhack/u-boot
 *
 * The scan only consults the top-level children of the specified directory.
 *
 * Each overlay includes complete board objects that entirely replace
 * the system-provided board objects in memory.  Some of the keys in
 * overlay board objects are computed at run-time and should not appear
 * in the plists loaded from the file system.
 *
 * The schema of the overlay board plists are as follows:
 *
 * <plist>
 * <dict>
 *	<!--
 *	  -- Key: string matching a "compatible" DT property.
 *	  -- Value: dictionary representing a board object.
 *	  -- (required)
 *	  -->
 *	<key>example,example-board</key>
 *	<dict>
 *		<!--
 *		  -- Key: "description".
 *		  -- Value: string containing the board description.
 *		  -- (required)
 *		  -->
 *		<key>description</key>
 *		<string>Example Co. Example Board</string>
 *
 *		<!--
 *		  -- Key: "u-boot-install".
 *		  --      (and variants; see discussion below)
 *		  --      "u-boot-install-emmc", etc.).
 *		  -- Value: Array of u-boot installation step objects,
 *		  --        as described below.
 *		  -- (required)
 *		  --
 *		  -- At least one of these objects is required.  If the
 *		  -- board uses a single set of steps for all boot media
 *		  -- types, then it should provide just "u-boot-install".
 *		  -- Otherwise, it whould provide one or more objects
 *		  -- with names reflecting the media type, e.g.:
 *		  --
 *		  --	"u-boot-install-sdmmc"	(for SD cards)
 *		  --	"u-boot-install-emmc"	(for eMMC modules)
 *		  --	"u-boot-install-usb"	(for USB block storage)
 *		  --	"u-boot-install-spi"	(for SPI NOR flash)
 *		  --
 *		  -- These installation steps will be selectable using
 *		  -- the "media=..." option to installboot(8).
 *		  -->
 *		<key>u-boot-install</key>
 *		<array>
 *			<!-- see installation object discussion below. -->
 *		</array>
 *
 *		<!--
 *		  -- Key: "runtime-u-boot-path"
 *		  -- Value: A string representing the path to the u-boot
 *		  --        binary files needed to install the boot loader.
 *		  --        This value is computed at run-time and is the
 *		  --        same directory in which the instalboot.plist
 *		  --        file for that u-boot package is located.
 *		  --        This key/value pair should never be included
 *		  --        in an installboot.plist file, and including it
 *		  --	    will cause the overlay to be rejected.
 *		  -- (computed at run-time)
 *		  -->
 *		<key>runtime-u-boot-path</key>
 *		<string>/usr/pkg/share/u-boot/exampleboard</string>
 *	</dict>
 * </dict>
 * </plist>
 *
 * The installation objects provide a description of the steps needed
 * to install u-boot on the boot media.  Each installation object it
 * itself an array of step object.
 *
 * A basic installation object has a single step that instructs
 * installboot(8) to write a file to a specific offset onto the
 * boot media.
 *
 *	<key>u-boot-install</key>
 *	<!-- installation object -->
 *	<array>
 *		<!-- step object -->
 *		<dict>
 *			<!--
 *			  -- Key: "file-name".
 *			  -- Value: a string naming the file to be
 *			  --        written to the media.
 *			  -- (required)
 *			  -->
 *			<key>file-name</key>
 *			<string>u-boot-with-spl.bin</string>
 *
 *			<!--
 *			  -- Key: "image-offset".
 *			  -- Value: an integer specifying the offset
 *			  --        into the output image or device
 *			  --        where to write the file.  Defaults
 *			  --        to 0 if not specified.
 *			  -- (optional)
 *			  -->
 *			<key>image-offset</key>
 *			<integer>8192</integer>
 *		</dict>
 *	</array>
 *
 * Some installations require multiple steps with special handling.
 *
 *	<key>u-boot-install</key>
 *	<array>
 *		<--
 *		 -- Step 1: Write the initial portion of the boot
 *		 -- loader onto the media.  The loader has a "hole"
 *		 -- to leave room for the MBR partition table.  Take
 *		 -- care not to scribble over the table.
 *		 -->
 *		<dict>
 *			<key>file-name</key>
 *			<string>u-boot-img.bin</string>
 *
 *			<!--
 *			  -- Key: "file-size".
 *			  -- Value: an integer specifying the amount of
 *			  --        data from the file to be written to the
 *			  --        output.  Defaults to "to end of file" if
 *			  --        not specified.
 *			  -- (optional)
 *			  -->
 *			<!-- Stop short of the MBR partition table. -->
 *			<key>file-size</key>
 *			<integer>442</integer>
 *
 *			<!--
 *			  -- Key: "preserve".
 *			  -- Value: a boolean indicating that any partial
 *			  --        output block should preserve any pre-
 *			  --        existing contents of that block for
 *			  --        the portion of the of the block not
 *			  --        overwritten by the input file.
 *			  --        (read-modify-write)
 *			  -- (optional)
 *			  -->
 *			<!-- Preserve the MBR partition table. -->
 *			<key>preserve</key>
 *			<true/>
 *		</dict>
 *		<--
 *		 -- Step 2: Write the rest of the loader after the
 *		 -- MBR partition table.
 *		 -->
 *		<dict>
 *			<key>file-name</key>
 *			<string>u-boot-img.bin</string>
 *
 *			<!--
 *			  -- Key: "file-offset".
 *			  -- Value: an integer specifying the offset into
 *			  --        the input file from where to start
 *			  --        copying to the output.
 *			  -- (optional)
 *			  -->
 *			<key>file-offset</key>
 *			<integer>512</integer>
 *
 *			<!-- ...just after the MBR partition talble. -->
 *			<key>image-offset</key>
 *			<integer>512</integer>
 *		</dict>
 *	</array>
 *
 * There are some addditional directives for installing on raw flash devices:
 *
 *	<key>u-boot-install-spi</key>
 *	<array>
 *		<!-- This board's SPI NOR flash is 16Mbit (2MB) in size,
 *		  -- arranged as 32 512Kbit (64KB) blocks.
 *		<dict>
 *			<key>file-name</key>
 *			<string>u-boot-with-spl.bin</string>
 *
 *			<!-- Key: "input-block-size".
 *			  -- Value: an integer specifying how much file
 *			  --        data to read per input block before
 *			  --        padding.  Must be used in conjunction
 *			  --        with "input-pad-size".
 *			  -- (optional)
 *			  -->
 *			<key>input-block-size</key>
 *			<integer>2048</integer>
 *
 *			<!-- Key: "input-pad-size".
 *			  -- Value: an integer specifing the amount of
 *			  --        zero padding inserted per input block.
 *			  --        Must be used in cojunction with
 *			  --        "input-block-size".
 *			  -- (optional)
 *			  -->
 *			<key>input-pad-size</key>
 *			<integer>2048</integer>
 *
 *			<!-- Key: "output-size".
 *			  -- Value: an integer specifying the total
 *			  --        size to be written to the output
 *			  --        device.  This is used when writing
 *			  --        a bootloader to a raw flash memory
 *			  --        device such as a SPI NOR flash.
 *			  --        The boot loader MUST fit within
 *			  --        this size and the output will be
 *			  --        padded to this size with zeros.
 *			  --
 *			  --        If the "output-block-size" key (below)
 *			  --        is also specified, then this value
 *			  --        must be a multiple of the output block
 *			  --        size.
 *			  -- (optional)
 *			  -->
 *			<key>output-size</key>
 *			<integer>2097152</integer>
 *
 *			<-- Key: "output-block-size"
 *			 -- Value: an integer specifying the size of
 *			 --        the blocks used to write to the
 *			 --        output device.  If the output device
 *			 --        simulates a disk block storage device,
 *			 --        then this value must be a multiple of
 *			 --        the reported sector size.
 *			 -- (optional)
 *			 -->
 *			<key>output-block-size</key>
 *			<integer>65536</integer>
 *		</dict>
 *	</array>
 *
 * For boards that require a media specification to be provided, it
 * may be the case that two media types have identical steps.  It
 * could be confusing for users to see a list of media types that does
 * not include the media type on which they are installing, so there
 * is an alias capability:
 *
 *	<key>u-boot-install-spi</key>
 *	<array>
 *		.
 *		.
 *		.
 *	</array>
 *	<key>u-boot-install-sdmmc</key>
 *	<array>
 *		.
 *		.
 *		.
 *	</array>
 *	<-- Steps for eMMC are identical to SDMMC on this board. -->
 *	<key>u-boot-install-emmc</key>
 *	<string>u-boot-install-sdmmc</string>
 */

/*
 * make_path --
 *	Build a path into the given buffer with the specified
 *	format.  Returns NULL if the path won't fit.
 */
static __printflike(3,4) const char *
make_path(char *buf, size_t bufsize, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(buf, bufsize, fmt, ap);
	va_end(ap);

	if (ret < 0 || (size_t)ret >= bufsize)
		return NULL;

	return buf;
}

#ifndef EVBOARDS_PLIST_BASE
#define	EVBOARDS_PLIST_BASE	"/usr"
#endif

static const char evb_db_base_location[] =
    EVBOARDS_PLIST_BASE "/share/installboot";

#ifndef DEFAULT_UBOOT_PKG_PATH
#define	DEFAULT_UBOOT_PKG_PATH	"/usr/pkg/share/u-boot"
#endif

#ifndef UBOOT_PATHS_ENV_VAR
#define	UBOOT_PATHS_ENV_VAR	"INSTALLBOOT_UBOOT_PATHS"
#endif

static const char evb_uboot_pkg_path[] = DEFAULT_UBOOT_PKG_PATH;

/*
 * evb_db_base_path --
 *	Returns the path to the base board db file.
 */
static const char *
evb_db_base_path(ib_params *params, char *buf, size_t bufsize)
{

	return make_path(buf, bufsize, "%s/%s/boards.plist",
	    evb_db_base_location, params->machine->name);
}

/*
 * evb_uboot_pkg_paths --
 *	Returns an array of u-boot package paths to scan for
 *	installboot.plist files.
 *
 *	Number of array elements, not including the NULL terminator,
 *	is returned in *countp.
 *
 *	The working buffer is returned in *bufp so that the caller
 *	can free it.
 */
static char **
evb_uboot_pkg_paths(ib_params *params, int *countp, void **bufp)
{
	char **ret_array = NULL;
	char *buf = NULL;
	const char *pathspec;
	int i, count;
	char *cp, *startcp;

	pathspec = getenv(UBOOT_PATHS_ENV_VAR);
	if (pathspec == NULL)
		pathspec = evb_uboot_pkg_path;

	if (strlen(pathspec) == 0)
		goto out;

	/* Count the path elements. */
	for (cp = __UNCONST(pathspec), count = 0;;) {
		count++;
		cp = strchr(cp, ':');
		if (cp == NULL)
			break;
		cp++;
	}

	buf = malloc((sizeof(char *) * (count + 1)) +
		     strlen(pathspec) + 1);
	if (buf == NULL)
		goto out;

	/*
	 * Because we want to follow the usual "paths are listed in priority
	 * order" semantics, we reverse the order of the paths when we put
	 * them into the array we feed to fts.  This is because we always
	 * overwrite existing entries as we find them, thus the last board
	 * object found one a given key is the one that will be used.
	 */

	ret_array = (char **)buf;
	startcp = buf + (sizeof(char *) * (count + 1));
	/* this is a safe strcpy(); don't replace it. */
	strcpy(startcp, pathspec);

	cp = strrchr(startcp, ':');
	if (cp == NULL)
		cp = startcp;

	for (i = 0;;) {
		if (*cp == ':') {
			ret_array[i++] = cp+1;
			*cp-- = '\0';
		} else
			ret_array[i++] = cp;
		if (cp == startcp)
			break;
		cp = strrchr(cp, ':');
		if (cp == NULL)
			cp = startcp;
	}
	assert(i == count);
	ret_array[i] = NULL;

 out:
	if (ret_array == NULL) {
		if (buf != NULL)
			free(buf);
	} else {
		if (countp != NULL)
			*countp = count;
		if (bufp != NULL)
			*bufp = buf;
	}
	return ret_array;
}

static const char step_file_name_key[] = "file-name";
static const char step_file_offset_key[] = "file-offset";
static const char step_file_size_key[] = "file-size";
static const char step_image_offset_key[] = "image-offset";
static const char step_input_block_size_key[] = "input-block-size";
static const char step_input_pad_size_key[] = "input-pad-size";
static const char step_output_size_key[] = "output-size";
static const char step_output_block_size_key[] = "output-block-size";
static const char step_preserve_key[] = "preserve";

static bool
validate_ubstep_object(evb_ubstep obj)
{
	/*
	 * evb_ubstep is a dictionary with the following keys:
	 *
	 *	"file-name"         (string) (required)
	 *	"file-offset"       (number) (optional)
	 *	"file-size"         (number) (optional)
	 *	"image-offset"      (number) (optional)
	 *	"input-block-size"  (number) (optional)
	 *	"input-pad-size"    (number) (optional)
	 *	"output-size"       (number) (optional)
	 *	"output-block-size" (number) (optional)
	 *	"preserve"          (bool)   (optional)
	 */
	if (prop_object_type(obj) != PROP_TYPE_DICTIONARY)
		return false;

	prop_object_t v;

	v = prop_dictionary_get(obj, step_file_name_key);
	if (v == NULL ||
	    prop_object_type(v) != PROP_TYPE_STRING)
	    	return false;

	v = prop_dictionary_get(obj, step_file_offset_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_NUMBER)
	    	return false;

	v = prop_dictionary_get(obj, step_file_size_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_NUMBER)
	    	return false;

	v = prop_dictionary_get(obj, step_image_offset_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_NUMBER)
	    	return false;

	bool have_input_block_size = false;
	bool have_input_pad_size = false;

	v = prop_dictionary_get(obj, step_input_block_size_key);
	if (v != NULL) {
		have_input_block_size = true;
		if (prop_object_type(v) != PROP_TYPE_NUMBER)
			return false;
	}

	v = prop_dictionary_get(obj, step_input_pad_size_key);
	if (v != NULL) {
		have_input_pad_size = true;
		if (prop_object_type(v) != PROP_TYPE_NUMBER)
			return false;
	}

	/* Must have both or neither of input-{block,pad}-size. */
	if (have_input_block_size ^ have_input_pad_size)
		return false;

	v = prop_dictionary_get(obj, step_output_size_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_NUMBER)
		return false;

	v = prop_dictionary_get(obj, step_output_block_size_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_NUMBER)
		return false;

	v = prop_dictionary_get(obj, step_preserve_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_BOOL)
	    	return false;

	return true;
}

static bool
validate_ubinstall_object(evb_board board, evb_ubinstall obj)
{
	/*
	 * evb_ubinstall is either:
	 * -- an array with one or more evb_ubstep objects.
	 * -- a string representing an alias of another evb_ubinstall
	 *    object
	 *
	 * (evb_ubsteps is just a convenience type for iterating
	 * over the steps.)
	 */

	if (prop_object_type(obj) == PROP_TYPE_STRING) {
		evb_ubinstall tobj = prop_dictionary_get(board,
		    prop_string_value((prop_string_t)obj));

		/*
		 * The target evb_ubinstall object must exist
		 * and must itself be a proper evb_ubinstall,
		 * not another alias.
		 */
		if (tobj == NULL ||
		    prop_object_type(tobj) != PROP_TYPE_ARRAY) {
			return false;
		}
		return true;
	}

	if (prop_object_type(obj) != PROP_TYPE_ARRAY)
		return false;
	if (prop_array_count(obj) < 1)
		return false;

	prop_object_t v;
	prop_object_iterator_t iter = prop_array_iterator(obj);

	while ((v = prop_object_iterator_next(iter)) != NULL) {
		if (!validate_ubstep_object(v))
			break;
	}

	prop_object_iterator_release(iter);
	return v == NULL;
}

static const char board_description_key[] = "description";
static const char board_u_boot_pkg_key[] = "u-boot-pkg";
static const char board_u_boot_path_key[] = "runtime-u-boot-path";
static const char board_u_boot_install_key[] = "u-boot-install";

static bool
validate_board_object(evb_board obj, bool is_overlay)
{
	/*
	 * evb_board is a dictionary with the following keys:
	 *
	 *	"description"		(string) (required)
	 *	"u-boot-pkg"		(string) (optional, base only)
	 *	"runtime-u-boot-path"	(string) (required, overlay only)
	 *
	 * With special consideration for these keys:
	 *
	 * Either this key and no other "u-boot-install*" keys:
	 *	"u-boot-install"	(string) (required, overlay only)
	 *
	 * Or one or more keys of the following pattern:
	 *	"u-boot-install-*"	(string) (required, overlay only)
	 */
	bool has_default_install = false;
	bool has_media_install = false;

	if (prop_object_type(obj) != PROP_TYPE_DICTIONARY)
		return false;

	prop_object_t v;

	v = prop_dictionary_get(obj, board_description_key);
	if (v == NULL ||
	    prop_object_type(v) != PROP_TYPE_STRING)
	    	return false;

	v = prop_dictionary_get(obj, board_u_boot_pkg_key);
	if (v != NULL &&
	    (is_overlay || prop_object_type(v) != PROP_TYPE_STRING))
	    	return false;

	/*
	 * "runtime-u-boot-path" is added to an overlay after we've
	 * validated the board object, so simply make sure it's not
	 * present.
	 */
	v = prop_dictionary_get(obj, board_u_boot_path_key);
	if (v != NULL)
		return false;

	prop_object_iterator_t iter = prop_dictionary_iterator(obj);
	prop_dictionary_keysym_t key;
	while ((key = prop_object_iterator_next(iter)) != NULL) {
		const char *cp = prop_dictionary_keysym_value(key);
		if (strcmp(cp, board_u_boot_install_key) == 0) {
			has_default_install = true;
		} else if (strncmp(cp, board_u_boot_install_key,
				   sizeof(board_u_boot_install_key) - 1) == 0 &&
			   cp[sizeof(board_u_boot_install_key) - 1] == '-') {
			has_media_install = true;
		} else {
			continue;
		}
		v = prop_dictionary_get_keysym(obj, key);
		assert(v != NULL);
		if (!is_overlay || !validate_ubinstall_object(obj, v))
			break;
	}
	prop_object_iterator_release(iter);
	if (key != NULL)
		return false;

	/*
	 * Overlays must have only a default install key OR one or more
	 * media install keys.
	 */
	if (is_overlay)
		return has_default_install ^ has_media_install;

	/*
	 * Base board objects must have neither.
	 */
	return (has_default_install | has_media_install) == false;
}

/*
 * evb_db_load_overlay --
 *	Load boards from an overlay file into the db.
 */
static void
evb_db_load_overlay(ib_params *params, const char *path,
    const char *runtime_uboot_path)
{
	prop_dictionary_t overlay;
	struct stat sb;

	if (params->flags & IB_VERBOSE)
		printf("Loading '%s'.\n", path);

	if (stat(path, &sb) < 0) {
		warn("'%s'", path);
		return;
	} else {
		overlay = prop_dictionary_internalize_from_file(path);
		if (overlay == NULL) {
			warnx("unable to parse overlay '%s'", path);
			return;
		}
	}

	/*
	 * Validate all of the board objects and add them to the board
	 * db, replacing any existing entries as we go.
	 */
	prop_object_iterator_t iter = prop_dictionary_iterator(overlay);
	prop_dictionary_keysym_t key;
	prop_dictionary_t board;
	while ((key = prop_object_iterator_next(iter)) != NULL) {
		board = prop_dictionary_get_keysym(overlay, key);
		assert(board != NULL);
		if (!validate_board_object(board, true)) {
			warnx("invalid board object in '%s': '%s'", path,
			    prop_dictionary_keysym_value(key));
			continue;
		}

		/* Add "runtime-u-boot-path". */
		prop_string_t string =
		    prop_string_create_copy(runtime_uboot_path);
		assert(string != NULL);
		prop_dictionary_set(board, board_u_boot_path_key, string);
		prop_object_release(string);

		/* Insert into board db. */
		prop_dictionary_set_keysym(params->mach_data, key, board);
	}
	prop_object_iterator_release(iter);
	prop_object_release(overlay);
}

/*
 * evb_db_load_overlays --
 *	Load the overlays from the search path.
 */
static void
evb_db_load_overlays(ib_params *params)
{
	char overlay_pathbuf[PATH_MAX+1];
	const char *overlay_path;
	char **paths;
	void *pathsbuf = NULL;
	FTS *fts;
	FTSENT *chp, *p;
	struct stat sb;

	paths = evb_uboot_pkg_paths(params, NULL, &pathsbuf);
	if (paths == NULL) {
		warnx("No u-boot search path?");
		return;
	}

	fts = fts_open(paths, FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR, NULL);
	if (fts == NULL ||
	    (chp = fts_children(fts, 0)) == NULL) {
		warn("Unable to search u-boot path");
		if (fts != NULL)
			fts_close(fts);
		return;
	}

	chp = fts_children(fts, 0);

	while ((p = fts_read(fts)) != NULL) {
		if (p->fts_info != FTS_D)
			continue;
		overlay_path = make_path(overlay_pathbuf,
		    sizeof(overlay_pathbuf), "%s/installboot.plist",
		    p->fts_path);
		if (overlay_path == NULL)
			continue;
		if (stat(overlay_path, &sb) < 0)
			continue;
		evb_db_load_overlay(params, overlay_path, p->fts_path);
	}

	fts_close(fts);

	/*
	 * If the user specifed a stage1 loader, then consult it last
	 * for a possible u-boot package location.
	 */
	if (params->stage1 != NULL) {
		overlay_path = make_path(overlay_pathbuf,
		    sizeof(overlay_pathbuf), "%s/installboot.plist",
		    params->stage1);
		if (overlay_path != NULL) {
			if (stat(overlay_path, &sb) == 0) {
				evb_db_load_overlay(params, overlay_path,
				    params->stage1);
			}
		}
	}
}

/*
 * evb_db_load_base --
 *	Load the base board db.
 */
static bool
evb_db_load_base(ib_params *params)
{
	char buf[PATH_MAX+1];
	const char *path;
	prop_dictionary_t board_db;
	struct stat sb;

	path = evb_db_base_path(params, buf, sizeof(buf));
	if (path == NULL)
		return false;

	if (params->flags & IB_VERBOSE)
		printf("Loading '%s'.\n", path);

	if (stat(path, &sb) < 0) {
		if (errno != ENOENT) {
			warn("'%s'", path);
			return false;
		}
		board_db = prop_dictionary_create();
		assert(board_db != NULL);
	} else {
		board_db = prop_dictionary_internalize_from_file(path);
		if (board_db == NULL) {
			warnx("unable to parse board db '%s'", path);
			return false;
		}
	}

	if (prop_dictionary_count(board_db) == 0) {
		/*
		 * Oh well, maybe we'll load some overlays.
		 */
		goto done;
	}

	/*
	 * Validate all of the board objects and remove any bad ones.
	 */
	prop_array_t all_board_keys = prop_dictionary_all_keys(board_db);
	prop_object_iterator_t iter = prop_array_iterator(all_board_keys);
	prop_dictionary_keysym_t key;
	prop_dictionary_t board;
	while ((key = prop_object_iterator_next(iter)) != NULL) {
		board = prop_dictionary_get_keysym(board_db, key);
		assert(board != NULL);
		if (!validate_board_object(board, false)) {
			warnx("invalid board object in '%s': '%s'", path,
			    prop_dictionary_keysym_value(key));
			prop_dictionary_remove_keysym(board_db, key);
		}
	}
	prop_object_iterator_release(iter);
	prop_object_release(all_board_keys);

 done:
	params->mach_data = board_db;
	return true;
}

/*
 * evb_db_load --
 *	Load the board database.
 */
bool
evb_db_load(ib_params *params)
{
	if (!evb_db_load_base(params))
		return false;
	evb_db_load_overlays(params);

	return true;
}

#if !HAVE_NBTOOL_CONFIG_H
/*
 * Native board name guessing methods.
 */

#ifdef SUPPORT_OPENFIRMWARE
static int
ofw_fd(void)
{
	static const char openfirm_path[] = "/dev/openfirm";

	return open(openfirm_path, O_RDONLY);
}

static int
OF_finddevice(const char *name)
{
	struct ofiocdesc ofio = {
		.of_name = __UNCONST(name),
		.of_namelen = strlen(name),
	};
	int fd = ofw_fd();

	if (fd == -1)
		return -1;

	if (ioctl(fd, OFIOCFINDDEVICE, &ofio) < 0) {
		if (errno != ENOENT)
			warn("OFIOCFINDDEVICE('%s')", name);
		ofio.of_nodeid = -1;
	}
	(void) close(fd);

	return ofio.of_nodeid;
}

static int
OF_getprop(int phandle, const char *prop, void *buf, size_t buflen)
{
	struct ofiocdesc ofio = {
		.of_nodeid = phandle,
		.of_name = __UNCONST(prop),
		.of_namelen = strlen(prop),
		.of_buf = buf,
		.of_buflen = buflen,
	};
	int fd = ofw_fd();

	if (fd == -1)
		return -1;

	int save_errno = 0;

	if (ioctl(fd, OFIOCGET, &ofio) < 0) {
		save_errno = errno;
		if (errno != ENOMEM && errno != ENOENT) {
			save_errno = errno;
			warn("OFIOCGET('%s')", prop);
		}
		ofio.of_buflen = -1;
	}
	(void) close(fd);
	errno = save_errno;

	return ofio.of_buflen;
}

static void *
ofw_getprop(int phandle, const char *prop, int *lenp)
{
	size_t buflen = 32;
	void *buf = NULL;
	int len;

	for (;;) {
		void *newbuf = realloc(buf, buflen);
		if (newbuf == NULL) {
			free(buf);
			return NULL;
		}
		buf = newbuf;
		switch (len = OF_getprop(phandle, prop, buf, buflen)) {
		case -1:
			if (errno != ENOMEM) {
				free(buf);
				return NULL;
			}
			buflen *= 2;
			break;

		default:
			if (lenp)
				*lenp = len;
			return buf;
		}
	}
}

static evb_board
evb_db_get_board_from_ofw(ib_params *params, const char **board_namep)
{
	int phandle;
	int compatible_len = 0;
	char *compatible_buf;
	const char *sp, *nsp;
	evb_board board;

	phandle = OF_finddevice("/");
	if (phandle == -1) {
		/* No OpenFirmware available. */
		return NULL;
	}

	compatible_buf = ofw_getprop(phandle, "compatible", &compatible_len);

	/*
	 * We just leak compatible_buf on success.  Not a big deal since
	 * we are not a long-running process.
	 */

	sp = compatible_buf;
	while (compatible_len &&
	       (nsp = memchr(sp, 0, compatible_len)) != NULL) {
		if (params->flags & IB_VERBOSE)
			printf("Checking OFW compatible string '%s'.\n", sp);
		board = prop_dictionary_get(params->mach_data, sp);
		if (board != NULL) {
			if (board_namep)
				*board_namep = sp;
			return board;
		}
		nsp++;	/* skip over NUL */
		compatible_len -= (nsp - sp);
		sp = nsp;
	}

	free(compatible_buf);
	return NULL;
}
#endif /* SUPPORT_OPENFIRMWARE */

#endif /* ! HAVE_NBTOOL_CONFIG_H */

/*
 * Host-tool and native board name guessing methods.
 */

#ifdef SUPPORT_FDT
static void *
load_dtb(ib_params *params)
{
	struct stat sb;
	void *buf;
	int fd;

	if (stat(params->dtb, &sb) < 0) {
		warn("%s", params->dtb);
		return NULL;
	}

	buf = malloc((size_t)sb.st_size);
	assert(buf != NULL);

	if ((fd = open(params->dtb, O_RDONLY)) < 0) {
		warn("%s", params->dtb);
		free(buf);
		return NULL;
	}

	if (read(fd, buf, (size_t)sb.st_size) != (ssize_t)sb.st_size) {
		warn("read '%s'", params->dtb);
		free(buf);
		buf = NULL;
	}
	(void) close(fd);

	return buf;
}

static evb_board
evb_db_get_board_from_dtb(ib_params *params, const char **board_namep)
{
	evb_board board = NULL;
	void *fdt = NULL;
	int error;

	fdt = load_dtb(params);
	if (fdt == NULL)
		return NULL;

	error = fdt_check_header(fdt);
	if (error) {
		warnx("%s: %s", params->dtb, fdt_strerror(error));
		goto bad;
	}

	const int system_root = fdt_path_offset(fdt, "/");
	if (system_root < 0) {
		warnx("%s: unable to find node '/'", params->dtb);
		goto bad;
	}

	const int system_ncompat = fdt_stringlist_count(fdt, system_root,
	    "compatible");
	if (system_ncompat <= 0) {
		warnx("%s: no 'compatible' property on node '/'", params->dtb);
		goto bad;
	}

	const char *compatible;
	int si;
	for (si = 0; si < system_ncompat; si++) {
		compatible = fdt_stringlist_get(fdt, system_root,
		    "compatible", si, NULL);
		if (compatible == NULL)
			continue;
		if (params->flags & IB_VERBOSE)
			printf("Checking FDT compatible string '%s'.\n",
			    compatible);
		board = prop_dictionary_get(params->mach_data, compatible);
		if (board != NULL) {
			/*
			 * We just leak compatible on success.  Not a big
			 * deal since we are not a long-running process.
			 */
			if (board_namep) {
				*board_namep = strdup(compatible);
				assert(*board_namep != NULL);
			}
			free(fdt);
			return board;
		}
	}

 bad:
	if (fdt != NULL)
		free(fdt);
	return NULL;
}
#endif /* SUPPORT_FDT */

/*
 * evb_db_get_board --
 *	Return the specified board object from the database.
 */
evb_board
evb_db_get_board(ib_params *params)
{
	const char *board_name = NULL;
	evb_board board = NULL;

#if !HAVE_NBTOOL_CONFIG_H
	/*
	 * If we're not a host tool, determine if we're running "natively".
	 */
	bool is_native = false;
	struct utsname utsname;

	if (uname(&utsname) < 0) {
		warn("uname");
	} else if (strcmp(utsname.machine, params->machine->name) == 0) {
		is_native = true;
	}
#endif /* ! HAVE_NBTOOL_CONFIG_H */

	/*
	 * Logic for determing board type that can be shared by host-tool
	 * and native builds goes here.
	 */

	/*
	 * Command-line argument trumps all.
	 */
	if (params->flags & IB_BOARD) {
		board_name = params->board;
	}

#ifdef SUPPORT_FDT
	if (board_name == NULL && (params->flags & IB_DTB)) {
		board = evb_db_get_board_from_dtb(params, &board_name);
		if ((params->flags & IB_VERBOSE) && board != NULL)
			printf("Found board '%s' from DTB data.\n", board_name);
#if !HAVE_NBTOOL_CONFIG_H
		/*
		 * If the user specified a DTB, then regardless of the
		 * outcome, this is like specifying the board directly,
		 * so native checks should be skipped.
		 */
		is_native = false;
#endif /* ! HAVE_NBTOOL_CONFIG_H */
	}
#endif /* SUPPORT_FDT */

#if !HAVE_NBTOOL_CONFIG_H
	/*
	 * Non-host-tool logic for determining the board type goes here.
	 */

#ifdef SUPPORT_OPENFIRMWARE
	if (board_name == NULL && is_native) {
		board = evb_db_get_board_from_ofw(params, &board_name);
		if ((params->flags & IB_VERBOSE) && board != NULL)
			printf("Found board '%s' from OFW data.\n", board_name);
	}
#endif /* SUPPORT_OPENFIRMWARE */

	/* Ensure is_native is consumed. */
	if (is_native == false)
		is_native = false;

#endif /* ! HAVE_NBTOOL_CONFIG_H */

	/*
	 * If all else fails, we can always rely on the user, right?
	 */
	if (board_name == NULL) {
		if (!(params->flags & IB_BOARD)) {
			warnx("Must specify board=...");
			return NULL;
		}
		board_name = params->board;
	}

	assert(board_name != NULL);

	if (board == NULL)
		board = prop_dictionary_get(params->mach_data, board_name);
	if (board == NULL)
		warnx("Unknown board '%s'", board_name);

	/* Ensure params->board is always valid. */
	params->board = board_name;

	if (params->flags & IB_VERBOSE) {
		printf("Board: %s\n", evb_board_get_description(params, board));
	}

	return board;
}

/*
 * evb_db_list_boards --
 *	Print the list of known boards to the specified output stream.
 */
void
evb_db_list_boards(ib_params *params, FILE *out)
{
	prop_object_iterator_t iter;
	prop_dictionary_keysym_t key;
	evb_board board;
	const char *uboot_pkg;
	const char *uboot_path;

	/*
	 * By default, we only list boards that we have a u-boot
	 * package installed for, or if we know which package you
	 * need to install.  You get the full monty in verbose mode.
	 */

	iter = prop_dictionary_iterator(params->mach_data);
	while ((key = prop_object_iterator_next(iter)) != NULL) {
		board = prop_dictionary_get_keysym(params->mach_data, key);
		assert(board != NULL);
		uboot_pkg = evb_board_get_uboot_pkg(params, board);
		uboot_path = evb_board_get_uboot_path(params, board);

		if (uboot_pkg == NULL && uboot_path == NULL &&
		    !(params->flags & IB_VERBOSE))
			continue;

		fprintf(out, "%-30s %s\n",
		    prop_dictionary_keysym_value(key),
		    evb_board_get_description(params, board));

		if ((params->flags & IB_VERBOSE) && uboot_path) {
			fprintf(out, "\t(u-boot package found at %s)\n",
			    uboot_path);
		} else if ((params->flags & IB_VERBOSE) && uboot_pkg) {
			fprintf(out,
			    "\t(install the sysutils/u-boot-%s package)\n",
			    uboot_pkg);
		}
	}
	prop_object_iterator_release(iter);
}

/*
 * evb_board_get_description --
 *	Return the description for the specified board.
 */
const char *
evb_board_get_description(ib_params *params, evb_board board)
{
	prop_string_t string;

	string = prop_dictionary_get(board, board_description_key);
	return prop_string_value(string);
}

/*
 * evb_board_get_uboot_pkg --
 *	Return the u-boot package name for the specified board.
 */
const char *
evb_board_get_uboot_pkg(ib_params *params, evb_board board)
{
	prop_string_t string;

	string = prop_dictionary_get(board, board_u_boot_pkg_key);
	if (string == NULL)
		return NULL;
	return prop_string_value(string);
}

/*
 * evb_board_get_uboot_path --
 *	Return the u-boot installed package path for the specified board.
 */
const char *
evb_board_get_uboot_path(ib_params *params, evb_board board)
{
	prop_string_t string;

	string = prop_dictionary_get(board, board_u_boot_path_key);
	if (string == NULL)
		return NULL;
	return prop_string_value(string);
}

/*
 * evb_board_get_uboot_install --
 *	Return the u-boot install object for the specified board,
 *	corresponding to the media specified by the user.
 */
evb_ubinstall
evb_board_get_uboot_install(ib_params *params, evb_board board)
{
	evb_ubinstall install;

	install = prop_dictionary_get(board, board_u_boot_install_key);

	if (!(params->flags & IB_MEDIA)) {
		if (install == NULL) {
			warnx("Must specify media=... for board '%s'",
			    params->board);
			goto list_media;
		}
		return install;
	}

	/* media=... was specified by the user. */

	if (install) {
		warnx("media=... is not a valid option for board '%s'",
		    params->board);
		return NULL;
	}

	char install_key[128];
	int n = snprintf(install_key, sizeof(install_key), "%s-%s",
	    board_u_boot_install_key, params->media);
	if (n < 0 || (size_t)n >= sizeof(install_key))
		goto invalid_media;
	install = prop_dictionary_get(board, install_key);
	if (install != NULL) {
		if (prop_object_type(install) == PROP_TYPE_STRING) {
			/*
			 * This is an alias.  Fetch the target.  We
			 * have already validated that the target
			 * exists.
			 */
			install = prop_dictionary_get(board,
			    prop_string_value((prop_string_t)install));
		}
		return install;
	}
 invalid_media:
	warnx("invalid media specification: '%s'", params->media);
 list_media:
	fprintf(stderr, "Valid media types:");
	prop_array_t array = evb_board_copy_uboot_media(params, board);
	assert(array != NULL);
	prop_object_iterator_t iter = prop_array_iterator(array);
	prop_string_t string;
	while ((string = prop_object_iterator_next(iter)) != NULL)
		fprintf(stderr, " %s", prop_string_value(string));
	fprintf(stderr, "\n");
	prop_object_iterator_release(iter);
	prop_object_release(array);

	return NULL;
}

/*
 * evb_board_copy_uboot_media --
 *	Return the valid media types for the given board as an array
 *	of strings.
 *
 *	Follows the create rule; caller is responsible for releasing
 *	the array.
 */
prop_array_t
evb_board_copy_uboot_media(ib_params *params, evb_board board)
{
	prop_array_t array = prop_array_create();
	prop_object_iterator_t iter = prop_dictionary_iterator(board);
	prop_string_t string;
	prop_dictionary_keysym_t key;
	const char *cp;

	assert(array != NULL);
	assert(iter != NULL);

	while ((key = prop_object_iterator_next(iter)) != NULL) {
		cp = prop_dictionary_keysym_value(key);
		if (strcmp(cp, board_u_boot_install_key) == 0 ||
		    strncmp(cp, board_u_boot_install_key,
			    sizeof(board_u_boot_install_key) - 1) != 0)
			continue;
		string = prop_string_create_copy(strrchr(cp, '-')+1);
		assert(string != NULL);
		prop_array_add(array, string);
		prop_object_release(string);
	}
	prop_object_iterator_release(iter);
	return array;
}

/*
 * evb_ubinstall_get_steps --
 *	Get the install steps for a given install object.
 */
evb_ubsteps
evb_ubinstall_get_steps(ib_params *params, evb_ubinstall install)
{
	return prop_array_iterator(install);
}

/*
 * evb_ubsteps_next_step --
 *	Return the next step in the install object.
 *
 *	N.B. The iterator is released upon termination.
 */
evb_ubstep
evb_ubsteps_next_step(ib_params *params, evb_ubsteps steps)
{
	prop_dictionary_t step = prop_object_iterator_next(steps);

	/* If we are out of steps, release the iterator. */
	if (step == NULL)
		prop_object_iterator_release(steps);
	
	return step;
}

/*
 * evb_ubstep_get_file_name --
 *	Returns the input file name for the step.
 */
const char *
evb_ubstep_get_file_name(ib_params *params, evb_ubstep step)
{
	prop_string_t string = prop_dictionary_get(step, step_file_name_key);
	return prop_string_value(string);
}

/*
 * evb_ubstep_get_file_offset --
 *	Returns the input file offset for the step.
 */
uint64_t
evb_ubstep_get_file_offset(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step, step_file_offset_key);
	if (number != NULL)
		return prop_number_unsigned_value(number);
	return 0;
}

/*
 * evb_ubstep_get_file_size --
 *	Returns the size of the input file to copy for this step, or
 *	zero if the remainder of the file should be copied.
 */
uint64_t
evb_ubstep_get_file_size(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step, step_file_size_key);
	if (number != NULL)
		return prop_number_unsigned_value(number);
	return 0;
}

/*
 * evb_ubstep_get_image_offset --
 *	Returns the offset into the destination image / device to
 *	copy the input file.
 */
uint64_t
evb_ubstep_get_image_offset(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step, step_image_offset_key);
	if (number != NULL)
		return prop_number_unsigned_value(number);
	return 0;
}

/*
 * evb_ubstep_get_input_block_size --
 *	Returns the input block size to use when reading the boot loader
 *	file.
 */
uint64_t
evb_ubstep_get_input_block_size(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step,
						   step_input_block_size_key);
	if (number != NULL)
		return prop_number_unsigned_value(number);
	return 0;
}

/*
 * evb_ubstep_get_input_pad_size --
 *	Returns the input pad size to use when reading the boot loader
 *	file.
 */
uint64_t
evb_ubstep_get_input_pad_size(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step,
						   step_input_pad_size_key);
	if (number != NULL)
		return prop_number_unsigned_value(number);
	return 0;
}

/*
 * evb_ubstep_get_output_size --
 *	Returns the total output size that will be written to the
 *	output device.
 */
uint64_t
evb_ubstep_get_output_size(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step, step_output_size_key);
	if (number != NULL)
		return prop_number_unsigned_value(number);
	return 0;
}

/*
 * evb_ubstep_get_output_block_size --
 *	Returns the block size that must be written to the output device.
 */
uint64_t
evb_ubstep_get_output_block_size(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step,
						   step_output_block_size_key);
	if (number != NULL)
		return prop_number_unsigned_value(number);
	return 0;
}

/*
 * evb_ubstep_preserves_partial_block --
 *	Returns true if the step preserves a partial block.
 */
bool
evb_ubstep_preserves_partial_block(ib_params *params, evb_ubstep step)
{
	prop_bool_t val = prop_dictionary_get(step, step_preserve_key);
	if (val != NULL)
		return prop_bool_true(val);
	return false;
}

/*
 * evb_uboot_file_path --
 *	Build a file path from the u-boot base path in the board object
 *	and the file name in the step object.
 */
static const char *
evb_uboot_file_path(ib_params *params, evb_board board, evb_ubstep step,
    char *buf, size_t bufsize)
{
	const char *base_path = evb_board_get_uboot_path(params, board);
	const char *file_name = evb_ubstep_get_file_name(params, step);

	if (base_path == NULL || file_name == NULL)
		return NULL;

	return make_path(buf, bufsize, "%s/%s", base_path, file_name);
}

/*
 * evb_uboot_do_step --
 *	Given a evb_ubstep, do the deed.
 */
static int
evb_uboot_do_step(ib_params *params, const char *uboot_file, evb_ubstep step)
{
	struct stat sb;
	int ifd = -1;
	char *blockbuf = NULL;
	off_t curoffset;
	off_t file_remaining;
	bool rv = false;

	uint64_t file_size = evb_ubstep_get_file_size(params, step);
	uint64_t file_offset = evb_ubstep_get_file_offset(params, step);
	uint64_t image_offset = evb_ubstep_get_image_offset(params, step);
	uint64_t output_size = evb_ubstep_get_output_size(params, step);
	size_t   output_block_size =
			(size_t)evb_ubstep_get_output_block_size(params, step);
	size_t   input_block_size =
			(size_t)evb_ubstep_get_input_block_size(params, step);
	size_t   input_pad_size =
			(size_t)evb_ubstep_get_input_pad_size(params, step);
	bool	 preserves_partial_block =
			evb_ubstep_preserves_partial_block(params, step);
	const char *uboot_file_name =
			evb_ubstep_get_file_name(params, step);

	if (input_block_size == 0 && output_block_size == 0) {
		if (params->flags & IB_VERBOSE) {
			printf("Defaulting input-block-size and "
			       "output-block-size to sectorsize "
			       "(%" PRIu32 ")\n", params->sectorsize);
		}
		input_block_size = output_block_size = params->sectorsize;
	} else if (input_block_size != 0 && output_block_size == 0) {
		if (params->flags & IB_VERBOSE) {
			printf("Defaulting output-block-size to "
			       "input-block-size (%zu)\n",
			       input_block_size);
		}
		output_block_size = input_block_size;
	} else if (output_block_size != 0 && input_block_size == 0) {
		if (params->flags & IB_VERBOSE) {
			printf("Defaulting input-block-size to "
			       "output-block-size (%zu)\n",
			       output_block_size);
		}
		input_block_size = output_block_size;
	}

	if (output_block_size % params->sectorsize) {
		warnx("output-block-size (%zu) is not a multiple of "
		      "device sector size (%" PRIu32 ")",
		      output_block_size, params->sectorsize);
		goto out;
	}

	if ((input_block_size + input_pad_size) > output_block_size) {
		warnx("input-{block+pad}-size (%zu) is larger than "
		      "output-block-size (%zu)",
		      input_block_size + input_pad_size,
		      output_block_size);
		goto out;
	}

	if (output_block_size % (input_block_size + input_pad_size)) {
		warnx("output-block-size (%zu) it not a multiple of "
		      "input-{block+pad}-size (%zu)",
		      output_block_size,
		      input_block_size + input_pad_size);
		goto out;
	}

	blockbuf = malloc(output_block_size);
	if (blockbuf == NULL)
		goto out;

	ifd = open(uboot_file, O_RDONLY);
	if (ifd < 0) {
		warn("open '%s'", uboot_file);
		goto out;
	}
	if (fstat(ifd, &sb) < 0) {
		warn("fstat '%s'", uboot_file);
		goto out;
	}

	if (file_size)
		file_remaining = (off_t)file_size;
	else
		file_remaining = sb.st_size - (off_t)file_offset;

	if (output_size == 0) {
		output_size = roundup(file_remaining, output_block_size);
	} else if ((uint64_t)file_remaining > output_size) {
		warnx("file size (%lld) is larger than output-size (%" PRIu64
		      ")", (long long)file_remaining, output_size);
		goto out;
	}

	if (params->flags & IB_VERBOSE) {
		if (file_offset) {
			printf("Writing '%s' %lld @ %" PRIu64
			       "to '%s' @  %" PRIu64 "\n",
			       uboot_file_name, (long long)file_remaining,
			       file_offset, params->filesystem, image_offset);
		} else {
			printf("Writing '%s' %lld to '%s' @ %" PRIu64 "\n",
			       uboot_file_name, (long long)file_remaining,
			       params->filesystem, image_offset);
		}
	}

	if (lseek(ifd, (off_t)file_offset, SEEK_SET) < 0) {
		warn("lseek '%s' @ %" PRIu64, uboot_file,
		    file_offset);
		goto out;
	}

	for (curoffset = (off_t)image_offset;
	     output_size != 0;
	     curoffset += output_block_size, output_size -= output_block_size) {

		size_t outblock_remaining;
		size_t this_inblock;
		char *fill;

		/*
		 * Initialize the output buffer.  We're either
		 * filling it with zeros, or we're preserving
		 * device contents that we don't overwrite.
		 */
		memset(blockbuf, 0, output_block_size);
		if (preserves_partial_block) {
			if (params->flags & IB_VERBOSE) {
				printf("(Reading '%s' -- %zu @ %lld)\n",
				       params->filesystem,
				       output_block_size,
				       (long long)curoffset);
			}
			if (pread(params->fsfd, blockbuf,
				  output_block_size, curoffset) < 0) {
				warn("pread '%s'", params->filesystem);
				goto out;
			}
		}

		/*
		 * Fill the output buffer with the file contents,
		 * interleaved with padding as necessary.  (If
		 * there is no file left, we're going to be left
		 * with padding to cover the output-size.)
		 */
		for (outblock_remaining = output_block_size, fill = blockbuf;
		     outblock_remaining != 0;
		     fill += input_block_size + input_pad_size,
		     outblock_remaining -= input_block_size + input_pad_size) {

			this_inblock = input_block_size;
			if ((off_t)this_inblock > file_remaining) {
				this_inblock = file_remaining;
			}

			if (this_inblock) {
				if (params->flags & IB_VERBOSE) {
					printf("(Reading '%s' -- %zu @ %lld)\n",
					       uboot_file_name,
					       this_inblock,
					       (long long)lseek(ifd, 0,
								SEEK_CUR));
				}
				if (read(ifd, fill, this_inblock)
				    != (ssize_t)this_inblock) {
					warn("read '%s'", uboot_file);
					goto out;
				}
				file_remaining -= this_inblock;
			}
		}

		if (params->flags & IB_VERBOSE) {
			printf("(Writing '%s' -- %zu @ %lld)\n",
			       params->filesystem,
			       output_block_size, (long long)curoffset);
		}
		if (!(params->flags & IB_NOWRITE) &&
		    pwrite(params->fsfd, blockbuf, output_block_size,
			   curoffset) != (ssize_t)output_block_size) {
			warn("pwrite '%s'", params->filesystem);
			goto out;
		}
	}

	/* Success! */
	rv = true;

 out:
	if (ifd != -1 && close(ifd) == -1)
		warn("close '%s'", uboot_file);
	if (blockbuf)
		free(blockbuf);
	return rv;
}

int
evb_uboot_setboot(ib_params *params, evb_board board)
{
	char uboot_filebuf[PATH_MAX+1];
	const char *uboot_file;
	struct stat sb;
	off_t max_offset = 0;

	/*
	 * If we don't have a u-boot path for this board, it means
	 * that a u-boot package wasn't found.  Prompt the user to
	 * install it.
	 */
	if (evb_board_get_uboot_path(params, board) == NULL) {
		warnx("No u-boot package found for board '%s'",
		    params->board);
		uboot_file = evb_board_get_uboot_pkg(params, board);
		if (uboot_file != NULL)
			warnx("Please install the sysutils/u-boot-%s package.",
			    uboot_file);
		return 0;
	}

	evb_ubinstall install = evb_board_get_uboot_install(params, board);
	evb_ubsteps steps;
	evb_ubstep step;

	if (install == NULL)
		return 0;

	/*
	 * First, make sure the files are all there.  While we're
	 * at it, calculate the largest byte offset that we will
	 * be writing.
	 */
	steps = evb_ubinstall_get_steps(params, install);
	while ((step = evb_ubsteps_next_step(params, steps)) != NULL) {
		uint64_t file_offset = evb_ubstep_get_file_offset(params, step);
		uint64_t file_size = evb_ubstep_get_file_size(params, step);
		uint64_t image_offset =
		    evb_ubstep_get_image_offset(params, step);
		uboot_file = evb_uboot_file_path(params, board, step,
		    uboot_filebuf, sizeof(uboot_filebuf));
		if (uboot_file == NULL)
			return 0;
		if (stat(uboot_file, &sb) < 0) {
			warn("%s", uboot_file);
			return 0;
		}
		if (!S_ISREG(sb.st_mode)) {
			warnx("%s: %s", uboot_file, strerror(EFTYPE));
			return 0;
		}
		off_t this_max;
		if (file_size)
			this_max = file_size;
		else
			this_max = sb.st_size - file_offset;
		this_max += image_offset;
		if (max_offset < this_max)
			max_offset = this_max;
	}

	/*
	 * Ok, we've verified that all of the files are there, and now
	 * max_offset points to the first byte that's available for a
	 * partition containing a file system.
	 */

	off_t rounded_max_offset = (off_t)(max_offset / params->sectorsize) *
	    params->sectorsize;
	if (rounded_max_offset != max_offset)
		rounded_max_offset += params->sectorsize;

	if (params->flags & IB_VERBOSE) {
		printf("Max u-boot offset (rounded): %lld (%lld)\n",
		    (long long)max_offset, (long long)rounded_max_offset);
		printf("First free block available for file systems: "
		    "%lld (0x%llx)\n",
		    (long long)rounded_max_offset / params->sectorsize,
		    (long long)rounded_max_offset / params->sectorsize);
	}

	/* XXX Check MBR table for overlapping partitions. */

	/*
	 * Now write each binary component to the appropriate location
	 * on disk.
	 */
	steps = evb_ubinstall_get_steps(params, install);
	while ((step = evb_ubsteps_next_step(params, steps)) != NULL) {
		uboot_file = evb_uboot_file_path(params, board, step,
		    uboot_filebuf, sizeof(uboot_filebuf));
		if (uboot_file == NULL)
			return 0;
		if (!evb_uboot_do_step(params, uboot_file, step))
			return 0;
	}

	return 1;
}

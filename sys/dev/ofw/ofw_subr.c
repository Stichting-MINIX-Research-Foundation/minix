/*	$NetBSD: ofw_subr.c,v 1.23 2013/10/25 14:32:10 jdc Exp $	*/

/*
 * Copyright 1998
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ofw_subr.c,v 1.23 2013/10/25 14:32:10 jdc Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <dev/ofw/openfirm.h>

#define	OFW_MAX_STACK_BUF_SIZE	256
#define	OFW_PATH_BUF_SIZE	512

/*
 * int of_decode_int(p)
 *
 * This routine converts OFW encoded-int datums
 * into the integer format of the host machine.
 *
 * It is primarily used to convert integer properties
 * returned by the OF_getprop routine.
 *
 * Arguments:
 *	p		pointer to unsigned char array which is an
 *			OFW-encoded integer.
 *
 * Return Value:
 *	Decoded integer value of argument p.
 *
 * Side Effects:
 *	None.
 */
int
of_decode_int(const unsigned char *p)
{
	unsigned int i = *p++ << 8;
	i = (i + *p++) << 8;
	i = (i + *p++) << 8;
	return (i + *p);
}

/*
 * int of_compatible(phandle, strings)
 *
 * This routine checks an OFW node's "compatible" entry to see if
 * it matches any of the provided strings.
 *
 * It should be used when determining whether a driver can drive
 * a partcular device.
 *
 * Arguments:
 *	phandle		OFW phandle of device to be checked for
 *			compatibility.
 *	strings		Array of containing expected "compatibility"
 *			property values, presence of any of which
 *			indicates compatibility.
 *
 * Return Value:
 *	-1 if none of the strings are found in phandle's "compatibility"
 *	property, or the index of the string in "strings" of the first
 *	string found in phandle's "compatibility" property.
 *
 * Side Effects:
 *	None.
 */
int
of_compatible(int phandle, const char * const *strings)
{
	int len, allocated, rv;
	char *buf;
	const char *sp, *nsp;

	len = OF_getproplen(phandle, "compatible");
	if (len <= 0)
		return (-1);

	if (len > OFW_MAX_STACK_BUF_SIZE) {
		buf = malloc(len, M_TEMP, M_WAITOK);
		allocated = 1;
	} else {
		buf = alloca(len);
		allocated = 0;
	}

	/* 'compatible' size should not change. */
	if (OF_getprop(phandle, "compatible", buf, len) != len) {
		rv = -1;
		goto out;
	}

	sp = buf;
	while (len && (nsp = memchr(sp, 0, len)) != NULL) {
		/* look for a match among the strings provided */
		for (rv = 0; strings[rv] != NULL; rv++)
			if (strcmp(sp, strings[rv]) == 0)
				goto out;

		nsp++;			/* skip over NUL char */
		len -= (nsp - sp);
		sp = nsp;
	}
	rv = -1;

out:
	if (allocated)
		free(buf, M_TEMP);
	return (rv);

}

/*
 * int of_packagename(phandle, buf, bufsize)
 *
 * This routine places the last component of an OFW node's name
 * into a user-provided buffer.
 *
 * It can be used during autoconfiguration to make printing of
 * device names more informative.
 *
 * Arguments:
 *	phandle		OFW phandle of device whose name name is
 *			desired.
 *	buf		Buffer to contain device name, provided by
 *			caller.  (For now, must be at least 4
 *			bytes long.)
 *	bufsize		Length of buffer referenced by 'buf', in
 *			bytes.
 *
 * Return Value:
 *	-1 if the device path name could not be obtained or would
 *	not fit in the allocated temporary buffer, or zero otherwise
 *	(meaning that the leaf node name was successfully extracted).
 *
 * Side Effects:
 *	If the leaf node name was successfully extracted, 'buf' is
 *	filled in with at most 'bufsize' bytes of the leaf node
 *	name.  If the leaf node was not successfully extracted, a
 *	somewhat meaningful string is placed in the buffer.  In
 *	either case, the contents of 'buf' will be NUL-terminated.
 */
int
of_packagename(int phandle, char *buf, int bufsize)
{
	char *pbuf;
	const char *lastslash;
	int l, rv;

	pbuf = malloc(OFW_PATH_BUF_SIZE, M_TEMP, M_WAITOK);
	l = OF_package_to_path(phandle, pbuf, OFW_PATH_BUF_SIZE);

	/* check that we could get the name, and that it's not too long. */
	if (l < 0 ||
	    (l == OFW_PATH_BUF_SIZE && pbuf[OFW_PATH_BUF_SIZE - 1] != '\0')) {
		if (bufsize >= 25)
			snprintf(buf, bufsize, "??? (phandle 0x%x)", phandle);
		else if (bufsize >= 4)
			strlcpy(buf, "???", bufsize);
		else
			panic("of_packagename: bufsize = %d is silly",
			    bufsize);
		rv = -1;
	} else {
		pbuf[l] = '\0';
		lastslash = strrchr(pbuf, '/');
		strlcpy(buf, (lastslash == NULL) ? pbuf : (lastslash + 1),
		    bufsize);
		rv = 0;
	}

	free(pbuf, M_TEMP);
	return (rv);
}

/* 
 * Find the first child of a given node that matches name. Does not recurse.
 */
int
of_find_firstchild_byname(int node, const char *name)
{
	char namex[32]; 
	int nn;
 
	for (nn = OF_child(node); nn; nn = OF_peer(nn)) {
		memset(namex, 0, sizeof(namex));
		if (OF_getprop(nn, "name", namex, sizeof(namex)) == -1)
			continue;
		if (strcmp(name, namex) == 0)
			return nn;
	}
	return -1;
}

/*
 * Find a give node by name.  Recurses, and seems to walk upwards too.
 */

int
of_getnode_byname(int start, const char *target)
{
	int node, next;
	char name[64];

	if (start == 0)
		start = OF_peer(0);

	for (node = start; node; node = next) {
		memset(name, 0, sizeof name);
		OF_getprop(node, "name", name, sizeof name - 1);
		if (strcmp(name, target) == 0)
			break;

		if ((next = OF_child(node)) != 0)
			continue;

		while (node) {
			if ((next = OF_peer(node)) != 0)
				break;
			node = OF_parent(node);
		}
	}

	/* XXX is this correct? */
	return node;
}

/*
 * Create a uint32_t integer property from an OFW node property.
 */

boolean_t
of_to_uint32_prop(prop_dictionary_t dict, int node, const char *ofname,
    const char *propname)
{
	uint32_t prop;

	if (OF_getprop(node, ofname, &prop, sizeof(prop)) != sizeof(prop))
		return FALSE;

	return(prop_dictionary_set_uint32(dict, propname, prop));
}

/*
 * Create a data property from an OFW node property.  Max size of 256bytes.
 */

boolean_t
of_to_dataprop(prop_dictionary_t dict, int node, const char *ofname,
    const char *propname)
{
	prop_data_t data;
	int len;
	uint8_t prop[256];
	boolean_t res;

	len = OF_getprop(node, ofname, prop, 256);
	if (len < 1)
		return FALSE;

	data = prop_data_create_data(prop, len);
	res = prop_dictionary_set(dict, propname, data);
	prop_object_release(data);
	return res;
}

/*
 * look at output-device, see if there's a Sun-typical video mode specifier as
 * in screen:r1024x768x60 attached. If found copy it into *buffer, otherwise
 * return NULL
 */

char *
of_get_mode_string(char *buffer, int len)
{
	int options;
	char *pos, output_device[256];

	/*
	 * finally, let's see if there's a video mode specified in
	 * output-device and pass it on so there's at least some way
	 * to program video modes
	 */
	options = OF_finddevice("/options");
	if ((options == 0) || (options == -1))
		return NULL;
	if (OF_getprop(options, "output-device", output_device, 256) == 0)
		return NULL;

	/* find the mode string if there is one */
	pos = strstr(output_device, ":r");
	if (pos == NULL)
		return NULL;
	strncpy(buffer, pos + 2, len);
	return buffer;
}

/*
 * Iterate over the subtree of a i2c controller node.
 * Add all sub-devices into an array as part of the controller's
 * device properties.
 * This is used by the i2c bus attach code to do direct configuration.
 */
void
of_enter_i2c_devs(prop_dictionary_t props, int ofnode, size_t cell_size)
{
	int node, len;
	char name[32], compatible[32];
	uint64_t reg64;
	uint32_t reg32;
	uint64_t addr;
	prop_array_t array = NULL;
	prop_dictionary_t dev;

	for (node = OF_child(ofnode); node; node = OF_peer(node)) {
		if (OF_getprop(node, "name", name, sizeof(name)) <= 0)
			continue;
		len = OF_getproplen(node, "reg");
		addr = 0;
		if (cell_size == 8 && len >= sizeof(reg64)) {
			if (OF_getprop(node, "reg", &reg64, sizeof(reg64))
			    < sizeof(reg64))
				continue;
			addr = reg64;
			/*
			 * The i2c bus number (0 or 1) is encoded in bit 33
			 * of the register, but we encode it in bit 8 of
			 * i2c_addr_t.
			 */
			if (addr & 0x100000000)
				addr = (addr & 0xff) | 0x100;
		} else if (cell_size == 4 && len >= sizeof(reg32)) {
			if (OF_getprop(node, "reg", &reg32, sizeof(reg32))
			    < sizeof(reg32))
				continue;
			addr = reg32;
		} else {
			continue;
		}
		addr >>= 1;
		if (addr == 0) continue;

		if (array == NULL)
			array = prop_array_create();

		dev = prop_dictionary_create();
		prop_dictionary_set_cstring(dev, "name", name);
		prop_dictionary_set_uint32(dev, "addr", addr);
		prop_dictionary_set_uint64(dev, "cookie", node);
		of_to_dataprop(dev, node, "compatible", "compatible");
		if (OF_getprop(node, "compatible", compatible,
		    sizeof(compatible)) > 0) {
			/* Set size for EEPROM's that we know about */
			if (strcmp(compatible, "i2c-at24c64") == 0)
				prop_dictionary_set_uint32(dev, "size", 8192);
			if (strcmp(compatible, "i2c-at34c02") == 0)
				prop_dictionary_set_uint32(dev, "size", 256);
		}
		prop_array_add(array, dev);
		prop_object_release(dev);
	}

	if (array != NULL) {
		prop_dictionary_set(props, "i2c-child-devices", array);
		prop_object_release(array);
	}

	prop_dictionary_set_bool(props, "i2c-indirect-config", false);
}

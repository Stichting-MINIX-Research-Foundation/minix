#!/bin/sh
#
#       $NetBSD: sunldgen.sh,v 1.1 2013/03/15 12:12:16 pooka Exp $
#

# To support the Sun linker we need to make it behave like the GNU linker
# for orphaned sections.  That means generating __start/__stop symbols
# for them and that means some nopoly-atheist-withoutfood level trickery.
# We enumerate all the section names we wish to generate symbols for.
# The good news is that it's unlikely for NetBSD to grow any more
# link sets, and even if it does, it'll be a build-time failure
# on Sun platforms so it's easy to catch and mend the list below.

LINKSETS='rump_components evcnts prop_linkpools modules sysctl_funcs
	  bufq_strats domains dkwedge_methods ieee80211_funcs'
	
exec 1> ldscript_sun.rump
printf '# $NetBSD: sunldgen.sh,v 1.1 2013/03/15 12:12:16 pooka Exp $\n\n$mapfile_version 2\nLOAD_SEGMENT rumpkern_linksets {'
for lset in ${LINKSETS}; do
	printf '\n\tASSIGN_SECTION { IS_NAME= link_set_start_%s };\n' $lset
	printf '\tASSIGN_SECTION { IS_NAME= link_set_%s };\n' $lset
	printf '\tASSIGN_SECTION { IS_NAME= link_set_stop_%s };\n' $lset
	printf '\tOS_ORDER+= link_set_start_%s\n' $lset
	printf '\t    link_set_%s\n' $lset
	printf '\t    link_set_stop_%s;\n' $lset
done
echo '};'

exec 1> linksyms_sun.c
printf '/* $NetBSD: sunldgen.sh,v 1.1 2013/03/15 12:12:16 pooka Exp $ */\n\n'
for lset in ${LINKSETS}; do
	printf 'int __start_link_set_%s[0]\n' $lset
	printf '\t__attribute__((__section__("link_set_start_%s")));\n' $lset
	printf 'int __link_set_dummy_%s[0]\n' $lset
	printf '\t__attribute__((__section__("link_set_%s")));\n' $lset
	printf 'int __stop_link_set_%s[0]\n' $lset
	printf '\t__attribute__((__section__("link_set_stop_%s")));\n\n' $lset
done

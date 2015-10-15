#! /bin/sh

tmp=$(mktemp -d ../netpgpverify-test.XXXXXX)

pax -rwpp . ${tmp}
cat > ${tmp}/config.h <<EOF
#ifndef CONFIG_H_
#define CONFIG_H_ 20141204

#ifndef __UNCONST
#define __UNCONST(a)   ((void *)(unsigned long)(const void *)(a))
#endif /* __UNCONST */

#ifndef USE_ARG
#define USE_ARG(x)       /*LINTED*/(void)&(x)
#endif /* USE_ARG */

#endif /* CONFIG_H_ */
EOF
(cd ${tmp} && env USETOOLS=no make -f Makefile.bsd && make -f Makefile.bsd tst)
rm -rf ${tmp}

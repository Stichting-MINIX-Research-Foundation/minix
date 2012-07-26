#!/bin/sh
#
# aliaslist.sh -- Generate `idnalias.conf.sample' file.
#

cpu_company_system="$1"
utf8_name="$2"

cat <<EOF
*.ISO_8859-1	ISO-8859-1
*.ISO_8859-2	ISO-8859-1
*.SJIS		Shift_JIS
*.Shift_JIS	Shift_JIS
ja_JP.EUC	EUC-JP
ko_KR.EUC	EUC-KR
*.big5		Big5
*.Big5		Big5
*.KOI8-R	KOI8-R
*.GB2312	GB2312
ja		EUC-JP
EOF

case "$cpu_company_system" in
*-*-hpux*)
    echo "japanese	Shift_JIS"
    ;;
*)
    echo "japanese	EUC-JP"
esac

if [ "x$utf8_name" != xUTF-8 ] ; then
    echo "UTF-8		$utf8_name"
fi

exit 0

exec 2>>/cpp.log
set -x
if [ $# -ne 1 ]
then	exec /usr/pkg/bin/clang "$@" -E - 
else	exec /usr/pkg/bin/clang "$@" -E 
fi

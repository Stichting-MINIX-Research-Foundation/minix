#!/bin/bash

ARCH=i386
MINIX_MODULES_MAPFILE=${MINIX_ROOT}/minix.mods.map
MINIX_LLVM_BIN_DIR=${MINIX_LLVM_DIR}/bin

[ -f ${MINIX_LLVM_DIR}/common.inc ] || cp ${MINIX_LLVM_DIR}/common.inc.default ${MINIX_LLVM_DIR}/common.inc
. ${MINIX_LLVM_DIR}/common.inc

# generate_modules_map()
#
# Generates the ${MINIX_MODULES_MAPFILE} file

function generate_modules_map()
{
	local TMPFILE="/tmp/.modules.map.tmp"
	local OUTFILE="${MINIX_MODULES_MAPFILE}"
	local currdir=`pwd`

	echo "Generating Minix modules map..." 1>&2
	cd ${MINIX_ROOT}
	grep -r "^PROG=" . --include=Makefile | sed -e "s/\s*//g" | sed -e "s/PROG=//g" > ${TMPFILE}

	cat ${TMPFILE} | sed -e "s/\.\///g" > ${TMPFILE}.1

	for l in `cat ${TMPFILE}.1`; do echo "`echo $l | cut -d: -f2`=`echo $l | cut -d: -f1`" | sed -e "s/\/Makefile//g"; done > ${TMPFILE}.2
	GREP_FILTER=$( cat minix/drivers/Makefile | awk "\$1 ~ /endif/ {N=0}; N==1 {print}; \$4 ~ /earm/ {N=1}" | sed "s/^\t//g" | sed "s/SUBDIR[^ ]* *//g" | sed "s/ \\\//g" | xargs | sed "s/ /= -e ^/g")
	# The blacklist has drivers not available on x86; ideally this would be extracted from the Makefiles
	blacklist=""
	blacklist="$blacklist -e ^bmp085="
	blacklist="$blacklist -e ^cat24c256="
	blacklist="$blacklist -e ^fb="
	blacklist="$blacklist -e ^gpio="
	blacklist="$blacklist -e ^i2c="
	blacklist="$blacklist -e ^lan8710a="
	blacklist="$blacklist -e ^sht21="
	blacklist="$blacklist -e ^tda19988="
	blacklist="$blacklist -e ^tps65217="
	blacklist="$blacklist -e ^tps65950="
	blacklist="$blacklist -e ^tsl2550="
	blacklist="$blacklist -e ^usbd="
        grep -v -e ^${GREP_FILTER}= -e ^memory= $blacklist ${TMPFILE}.2 > ${OUTFILE}
	echo memory=minix/drivers/storage/memory >> ${OUTFILE} # memory always last

	rm -rf ${TMPFILE} ${TMPFILE}.1 ${TMPFILE}.2

	cd ${currdir}
}

# get_modules_path
#
# Searches through the modules map and gets all the locations
# pertaining to the module(s) being searched.

function get_modules_path()
{
	local MODULE_NAME=$1
	if [ ! -f "${MINIX_MODULES_MAPFILE}" ]; then
		generate_modules_map
	fi
	if [ $MODULE_NAME == "servers" ] || [ $MODULE_NAME == "drivers" ]; then
		MODULE_NAME="=minix/${MODULE_NAME}/"
	else
		MODULE_NAME="^${MODULE_NAME}="
	fi

	echo `grep "${MODULE_NAME}" ${MINIX_MODULES_MAPFILE} | cut -d= -f2`
}

# get_module_name
#
# Given a module path, it gives its corresponding module name

function get_module_name()
{
	local MODULE_PATH=$1
	if [ ! -f "${MINIX_MODULES_MAPFILE}" ]; then
		generate_modules_map
	fi

	echo `grep "${MODULE_PATH}$" ${MINIX_MODULES_MAPFILE} | cut -d= -f1`
}

# clean_module()
#
# Cleans up the DESTDIR directory for the specified module

function clean_module()
{
	local MODULE_NAME=$1
	local MODULE_PATH=$2
	local MODE=$3			# MODE can either be "relink" or "build"
	local currdir=`pwd`

	# By default, clean only the potentially instrumented files
	local TARGETS="${MODULE_NAME} *.opt.bcl *.bcl.o"

	if [ "${MODE}" == "relink" ]; then
		TARGETS="${TARGETS} *.bcl"
	fi

	if [ -d ${DESTDIR}/${MODULE_PATH} ]; then 
		cd ${DESTDIR}/${MODULE_PATH}
		rm -rf ${TARGETS} 2> /dev/null || true
	fi

	cd ${currdir}  
}


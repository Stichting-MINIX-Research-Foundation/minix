#!/bin/sh

cd $(dirname $0)

: ${NETBSDSRCDIR=${PWD}/../..}
: ${LLVMSRCDIR=${NETBSDSRCDIR}/external/bsd/llvm/dist}
: ${ARCH=i386}
: ${JOBS=1}
: ${OBJ_LLVM=${NETBSDSRCDIR}/../obj_llvm.${ARCH}}
: ${OBJ=${NETBSDSRCDIR}/../obj.${ARCH}}
: ${CROSS_TOOLS=${OBJ}/"tooldir.`uname -s`-`uname -r`-`uname -m`"/bin}
: ${MAKE=make}

echo ${NETBSDSRCDIR}
echo ${LLVMSRCDIR}
echo ${OBJ_LLVM}
echo ${OBJ}
echo ${CROSS_TOOLS}

# Retrieve all the GPL sources
cd ${NETBSDSRCDIR}
find . -name fetch.sh -exec '{}' \;

# Build LLVM manually
mkdir -p  ${OBJ_LLVM}
cd  ${OBJ_LLVM}

${LLVMSRCDIR}/llvm/configure \
    --enable-targets=x86 \
    --with-c-include-dirs=/usr/include/clang-3.6:/usr/include \
    --disable-timestamps \
    --prefix=/usr \
    --sysconfdir=/etc/llvm \
    --with-clang-srcdir=${LLVMSRCDIR}/clang \
    --host=i586-elf32-minix \
    --with-binutils-include=${NETBSDSRCDIR}/external/gpl3/binutils/dist/include \
    --disable-debug-symbols \
    --enable-assertions \
    --enable-bindings=none \
    llvm_cv_gnu_make_command=${MAKE} \
    ac_cv_path_CIRCO="echo circo" \
    ac_cv_path_DOT="echo dot" \
    ac_cv_path_DOTTY="echo dotty" \
    ac_cv_path_FDP="echo fdp" \
    ac_cv_path_NEATO="echo neato" \
    ac_cv_path_TWOPI="echo twopi" \
    ac_cv_path_XDOT="echo xdot" \
    --enable-optimized 

${MAKE} -j ${JOBS}

# Copy the gold plugin where the NetBSD build system expects it.
mkdir -p ${NETBSDSRCDIR}/minix/llvm/bin/
cp ${OBJ_LLVM}/./Release+Asserts/lib/libLTO.so   ${NETBSDSRCDIR}/minix/llvm/bin/
cp ${OBJ_LLVM}/./Release+Asserts/lib/LLVMgold.so ${NETBSDSRCDIR}/minix/llvm/bin/

# Copy useful LLVM tools
mkdir -p ${CROSS_TOOLS}
cp ${OBJ_LLVM}/./Release+Asserts/bin/llc    ${CROSS_TOOLS}
cp ${OBJ_LLVM}/./Release+Asserts/bin/opt    ${CROSS_TOOLS}
cp ${OBJ_LLVM}/./Release+Asserts/bin/llvm-* ${CROSS_TOOLS}

# Generate and Install default MINIX passes
cd ${NETBSDSRCDIR}/minix/llvm/passes/WeakAliasModuleOverride
${MAKE} install

cd ${NETBSDSRCDIR}/minix/llvm/passes/hello
${MAKE} install

cd ${NETBSDSRCDIR}/minix/llvm/passes/sectionify
${MAKE} install

cd ${NETBSDSRCDIR}/minix/llvm/passes/magic
${MAKE} install

cd ${NETBSDSRCDIR}/minix/llvm/passes/asr
${MAKE} install

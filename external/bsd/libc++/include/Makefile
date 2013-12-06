#	$NetBSD: Makefile,v 1.5 2013/11/28 13:47:29 joerg Exp $

.include <bsd.init.mk>

.PATH: ${LIBCXX_SRCDIR}/include
.PATH: ${LIBCXX_SRCDIR}/include/experimental
.PATH: ${LIBCXX_SRCDIR}/include/ext

INCS=	__bit_reference \
	__config \
	__debug \
	__functional_03 \
	__functional_base \
	__functional_base_03 \
	__hash_table \
	__locale \
	__mutex_base \
	__split_buffer \
	__sso_allocator \
	__std_stream \
	__tree \
	__tuple \
	__tuple_03 \
	__undef_min_max \
	algorithm \
	array \
	atomic \
	bitset \
	cassert \
	ccomplex \
	cctype \
	cerrno \
	cfenv \
	cfloat \
	chrono \
	cinttypes \
	ciso646 \
	climits \
	clocale \
	cmath \
	codecvt \
	complex \
	complex.h \
	condition_variable \
	csetjmp \
	csignal \
	cstdarg \
	cstdbool \
	cstddef \
	cstdint \
	cstdio \
	cstdlib \
	cstring \
	ctgmath \
	ctime \
	cwchar \
	cwctype \
	deque \
	dynarray \
	exception \
	forward_list \
	fstream \
	functional \
	future \
	initializer_list \
	iomanip \
	ios \
	iosfwd \
	iostream \
	istream \
	iterator \
	limits \
	list \
	locale \
	map \
	memory \
	mutex \
	new \
	numeric \
	optional \
	ostream \
	queue \
	random \
	ratio \
	regex \
	scoped_allocator \
	set \
	shared_mutex \
	sstream \
	stack \
	stdexcept \
	streambuf \
	string \
	strstream \
	system_error \
	tgmath.h \
	thread \
	tuple \
	typeindex \
	typeinfo \
	type_traits \
	unordered_map \
	unordered_set \
	utility \
	valarray \
	vector \
	__hash \
	hash_map \
	hash_set

INCS+=		cxxabi.h

INCSDIR=	/usr/include/c++
.for f in __hash hash_map hash_set
INCSDIR_${f}=	/usr/include/c++/ext
.endfor

.for f in dynarray optional
INCSDIR_${f}=	/usr/include/c++/experimental
.endfor

cxxabi.h: ${LIBCXXRT_SRCDIR}/src/cxxabi.h
	cp ${LIBCXXRT_SRCDIR}/src/cxxabi.h .

DPSRCS+=	cxxabi.h
CLEANFILES+=	cxxabi.h

.include <bsd.inc.mk>
.include <bsd.clean.mk>
.include <bsd.obj.mk>

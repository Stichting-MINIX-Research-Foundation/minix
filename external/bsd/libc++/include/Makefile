#	$NetBSD: Makefile,v 1.7 2015/08/20 10:06:21 joerg Exp $

.include <bsd.init.mk>

.PATH: ${LIBCXX_SRCDIR}/include

INCSDIR=	/usr/include/c++

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
	__undef___deallocate \
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
	vector

INCS+=		cxxabi.h

INCS+=	ext/__hash \
	ext/hash_map \
	ext/hash_set
INCS+=	experimental/__config \
	experimental/algorithm \
	experimental/chrono \
	experimental/dynarray \
	experimental/optional \
	experimental/ratio \
	experimental/string_view \
	experimental/system_error \
	experimental/tuple \
	experimental/type_traits \
	experimental/utility

cxxabi.h: ${LIBCXXRT_SRCDIR}/src/cxxabi.h
	cp ${LIBCXXRT_SRCDIR}/src/cxxabi.h .

DPSRCS+=	cxxabi.h
CLEANFILES+=	cxxabi.h

.include <bsd.inc.mk>
.include <bsd.clean.mk>
.include <bsd.obj.mk>

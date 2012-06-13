
ASM=	compat__login_getpwclass50.S compat__login50.S compat__loginx50.S \
	compat__pw_copy50.S compat__pw_copyx50.S \
	compat__pw_getpwconf50.S compat__setusercontext50.S
SRCS+=	${ASM}
CLEANFILES+= ${ASM}

.include "../generateasm.mk"

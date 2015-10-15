# $Id: phony-end.mk,v 1.1 2014/08/21 13:44:51 apb Exp $

all ok also.ok bug phony:
	@echo '${.TARGET .PREFIX .IMPSRC:L:@v@$v="${$v}"@}'

.END:	ok also.ok bug

phony bug:	.PHONY
all: phony

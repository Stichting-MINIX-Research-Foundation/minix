
all:
	cd float && make
	cd fphook && make
	cd `arch` && make
	cd libm2 && make
	cd libp && make
	cd liby && make
	cd math && make
	cd rts && make


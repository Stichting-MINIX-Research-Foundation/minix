
NVI=nvi-1.79
NVIWORK=$(NVI)/minix

all install:
	-mkdir $(NVIWORK)
	cd $(NVI) && make  $@

clean:
	if [ -f $(NVIWORK)/Makefile ]; then cd $(NVIWORK) && make clean; fi



NVI=nvi-1.79
NVIWORK=$(NVI)/minix

all:
	-mkdir $(NVIWORK)
	cd $(NVIWORK) && /bin/sh makeme.sh 

install: all
	cd $(NVIWORK) && sh ../build/install.minix

clean:
	if [ -f $(NVIWORK)/Makefile ]; then cd $(NVIWORK) && make clean; fi


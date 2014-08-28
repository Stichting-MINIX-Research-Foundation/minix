all: requestor grantor 1fifo 2fifo
	chmod +x down run

requestor: requestor.c inc.h
	cc -static -o $@ $< -lsys 

grantor: grantor.c inc.h
	cc -static -o $@ $< -lsys

1fifo 2fifo:
	mkfifo $@

run: all
	sh run

kill:
	sh down

clean:
	rm -f grantor requestor 1fifo 2fifo


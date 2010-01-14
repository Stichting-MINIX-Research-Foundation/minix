all: dstest subs
	chmod +x down run

dstest: dstest.c inc.h
	cc -o $@ $< -lsys

subs: subs.c inc.h
	cc -o $@ $< -lsys

run: all
	sh run

kill:
	sh down

clean:
	rm -f dstest subs


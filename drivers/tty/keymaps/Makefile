# Generate binary keymaps.

LK =	/usr/lib/keymaps

.SUFFIXES:	.src .map

.src.map:
	$(CC) -DKEYSRC=\"$<\" genmap.c
	./a.out > $@
	@rm -f a.out

all:	\
	dvorak.map \
	french.map \
	german.map \
	italian.map \
	japanese.map \
	latin-america.map \
	olivetti.map \
	polish.map \
	scandinavian.map \
	spanish.map \
	uk.map \
	us-std.map \
	us-swap.map \

install:	\
	$(LK) \
	$(LK)/dvorak.map \
	$(LK)/french.map \
	$(LK)/german.map \
	$(LK)/italian.map \
	$(LK)/japanese.map \
	$(LK)/latin-america.map \
	$(LK)/olivetti.map \
	$(LK)/polish.map \
	$(LK)/scandinavian.map \
	$(LK)/spanish.map \
	$(LK)/uk.map \
	$(LK)/us-std.map \
	$(LK)/us-swap.map \

clean:
	rm -f a.out *.map

$(LK):
	install -d $@

$(LK)/dvorak.map:	dvorak.map
	install -c $? $@

$(LK)/french.map:	french.map
	install -c $? $@

$(LK)/german.map:	german.map
	install -c $? $@

$(LK)/italian.map:	italian.map
	install -c $? $@

$(LK)/japanese.map:	japanese.map
	install -c $? $@

$(LK)/latin-america.map:	latin-america.map
	install -c $? $@

$(LK)/olivetti.map:	olivetti.map
	install -c $? $@

$(LK)/polish.map:	polish.map
	install -c $? $@

$(LK)/scandinavian.map:	scandinavian.map
	install -c $? $@

$(LK)/spanish.map:	spanish.map
	install -c $? $@

$(LK)/uk.map:	uk.map
	install -c $? $@

$(LK)/us-std.map:	us-std.map
	install -c $? $@

$(LK)/us-swap.map:	us-swap.map
	install -c $? $@

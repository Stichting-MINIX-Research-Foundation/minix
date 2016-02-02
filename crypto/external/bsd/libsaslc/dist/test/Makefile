# $NetBSD: Makefile,v 1.4 2011/02/12 23:21:33 christos Exp $

CC=gcc
SRC_PREFIX=../src
CFLAGS=-I../include -I$(SRC_PREFIX) -D SASLC_TEST_DIR=\"`pwd`/\"
ATFLIBS=-latf-c -lssl
LDFLAGS=`pwd`/$(SRC_PREFIX)/libsaslc.so -ggdb
TEST_CASES_C=t_dict t_session t_crypto t_error t_saslc t_parser
TEST_CASES=$(TEST_CASES_C)
VALGRIND=valgrind
VALGRIND_OPTS=--leak-check=full -q

all: build

build: $(TEST_CASES_SH) $(TEST_CASES_C) example_client

$(TEST_CASES_SH):
	@echo "building $@"
	@$(ENV) atf-compile -o $@ $@.sh

$(TEST_CASES_C):
	@echo "building $@"
	@$(CC) -o $@.o -c $@.c $(CFLAGS) -I/usr/include/local
	@$(CC) -o $@ $@.o $(LDFLAGS) $(ATFLIBS)

atf:
	@echo ""
	@echo "*************************************"
	@echo "* ATF TESTS:                        *"
	@echo "*************************************"
	@echo ""
	@$(ENV) atf-run -v test_dir=`pwd` | $(ENV) atf-report

valgrind:
	@echo ""
	@echo "*************************************"
	@echo "* VALGRIND TESTS:                   *"
	@echo "*************************************"
	@echo ""
	@for prog in $(TEST_CASES); do \
		echo "$$prog:"; \
		for testcase in `./$$prog -l | grep ident | awk {'print $$2'}`; do \
			echo " * $$testcase"; \
			$(VALGRIND) $(VALGRIND_OPTS) ./$$prog $$testcase ; \
		done \
	done

tests: build atf valgrind

example_client: example_client.c

clean:
	@rm -f $(TEST_CASES_C) $(TEST_CASES_SH) *.o resfile \
	    example_client

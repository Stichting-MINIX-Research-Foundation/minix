/*	$NetBSD: test_alloc.c,v 1.1.1.3 2014/07/12 11:57:48 spz Exp $	*/
/*
 * Copyright (c) 2007,2009,2012 by Internet Systems Consortium, Inc. ("ISC")
 *
 * We test the functions provided in alloc.c here. These are very
 * basic functions, and it is very important that they work correctly.
 *
 * You can see two different styles of testing:
 *
 * - In the first, we have a single test for each function that tests
 *   all of the possible ways it can operate. (This is the case for
 *   the buffer tests.)
 *
 * - In the second, we have a separate test for each of the ways a
 *   function can operate. (This is the case for the data_string
 *   tests.)
 *
 * The advantage of a single test per function is that you have fewer
 * tests, and less duplicated and extra code. The advantage of having
 * a separate test is that each test is simpler. Plus if you need to
 * allow certain tests to fail for some reason (known bugs that are
 * hard to fix for example), then
 */

/** @TODO: dmalloc() test */

#include "config.h"
#include <atf-c.h>
#include "dhcpd.h"

ATF_TC(buffer_allocate);

ATF_TC_HEAD(buffer_allocate, tc) {
    atf_tc_set_md_var(tc, "descr", "buffer_allocate basic test");
}

ATF_TC_BODY(buffer_allocate, tc) {
    struct buffer *buf = 0;

    /*
     * Check a 0-length buffer.
     */
    buf = NULL;
    if (!buffer_allocate(&buf, 0, MDL)) {
        atf_tc_fail("failed on 0-len buffer");
    }
    if (!buffer_dereference(&buf, MDL)) {
        atf_tc_fail("buffer_dereference() failed");
    }
    if (buf != NULL) {
        atf_tc_fail("buffer_dereference() did not NULL-out buffer");
    }

    /*
     * Check an actual buffer.
     */
    buf = NULL;
    if (!buffer_allocate(&buf, 100, MDL)) {
        atf_tc_fail("failed on allocate 100 bytes\n");
    }
    if (!buffer_dereference(&buf, MDL)) {
        atf_tc_fail("buffer_dereference() failed");
    }
    if (buf != NULL) {
        atf_tc_fail("buffer_dereference() did not NULL-out buffer");
    }

    /*
     * Okay, we're happy.
     */
    atf_tc_pass();
}

ATF_TC(buffer_reference);

ATF_TC_HEAD(buffer_reference, tc) {
    atf_tc_set_md_var(tc, "descr", "buffer_reference basic test");
}

ATF_TC_BODY(buffer_reference, tc) {

    struct buffer *a, *b;

    /*
     * Create a buffer.
     */
    a = NULL;
    if (!buffer_allocate(&a, 100, MDL)) {
        atf_tc_fail("failed on allocate 100 bytes");
    }

    /**
     * Confirm buffer_reference() doesn't work if we pass in NULL.
     *
     * @TODO: we should confirm we get an error message here.
     */
    if (buffer_reference(NULL, a, MDL)) {
        atf_tc_fail("succeeded on an error input");
    }

    /**
     * @TODO: we should confirm we get an error message if we pass
     *       a non-NULL target.
     */

    /*
     * Confirm we work under normal circumstances.
     */
    b = NULL;
    if (!buffer_reference(&b, a, MDL)) {
        atf_tc_fail("buffer_reference() failed");
    }

    if (b != a) {
        atf_tc_fail("incorrect pointer returned");
    }

    if (b->refcnt != 2) {
        atf_tc_fail("incorrect refcnt");
    }

    /*
     * Clean up.
     */
    if (!buffer_dereference(&b, MDL)) {
        atf_tc_fail("buffer_dereference() failed");
    }
    if (!buffer_dereference(&a, MDL)) {
        atf_tc_fail("buffer_dereference() failed");
    }

}


ATF_TC(buffer_dereference);

ATF_TC_HEAD(buffer_dereference, tc) {
    atf_tc_set_md_var(tc, "descr", "buffer_dereference basic test");
}

ATF_TC_BODY(buffer_dereference, tc) {
    struct buffer *a, *b;

    /**
     * Confirm buffer_dereference() doesn't work if we pass in NULL.
     *
     * TODO: we should confirm we get an error message here.
     */
    if (buffer_dereference(NULL, MDL)) {
        atf_tc_fail("succeeded on an error input");
    }

    /**
     * Confirm buffer_dereference() doesn't work if we pass in
     * a pointer to NULL.
     *
     * @TODO: we should confirm we get an error message here.
     */
    a = NULL;
    if (buffer_dereference(&a, MDL)) {
        atf_tc_fail("succeeded on an error input");
    }

    /*
     * Confirm we work under normal circumstances.
     */
    a = NULL;
    if (!buffer_allocate(&a, 100, MDL)) {
        atf_tc_fail("failed on allocate");
    }
    if (!buffer_dereference(&a, MDL)) {
        atf_tc_fail("buffer_dereference() failed");
    }
    if (a != NULL) {
        atf_tc_fail("non-null buffer after buffer_dereference()");
    }

    /**
     * Confirm we get an error from negative refcnt.
     *
     * @TODO: we should confirm we get an error message here.
     */
    a = NULL;
    if (!buffer_allocate(&a, 100, MDL)) {
        atf_tc_fail("failed on allocate");
    }
    b = NULL;
    if (!buffer_reference(&b, a, MDL)) {
        atf_tc_fail("buffer_reference() failed");
    }
    a->refcnt = 0;	/* purposely set to invalid value */
    if (buffer_dereference(&a, MDL)) {
        atf_tc_fail("buffer_dereference() succeeded on error input");
    }
    a->refcnt = 2;
    if (!buffer_dereference(&b, MDL)) {
        atf_tc_fail("buffer_dereference() failed");
    }
    if (!buffer_dereference(&a, MDL)) {
        atf_tc_fail("buffer_dereference() failed");
    }
}

ATF_TC(data_string_forget);

ATF_TC_HEAD(data_string_forget, tc) {
    atf_tc_set_md_var(tc, "descr", "data_string_forget basic test");
}

ATF_TC_BODY(data_string_forget, tc) {
    struct buffer *buf;
    struct data_string a;
    const char *str = "Lorem ipsum dolor sit amet turpis duis.";

    /*
     * Create the string we want to forget.
     */
    memset(&a, 0, sizeof(a));
    a.len = strlen(str);
    buf = NULL;
    if (!buffer_allocate(&buf, a.len, MDL)) {
        atf_tc_fail("out of memory");
    }
    if (!buffer_reference(&a.buffer, buf, MDL)) {
        atf_tc_fail("buffer_reference() failed");
    }
    a.data = a.buffer->data;
    memcpy(a.buffer->data, str, a.len);

    /*
     * Forget and confirm we've forgotten.
     */
    data_string_forget(&a, MDL);

    if (a.len != 0) {
        atf_tc_fail("incorrect length");
    }

    if (a.data != NULL) {
        atf_tc_fail("incorrect data");
    }
    if (a.terminated) {
        atf_tc_fail("incorrect terminated");
    }
    if (a.buffer != NULL) {
        atf_tc_fail("incorrect buffer");
    }
    if (buf->refcnt != 1) {
        atf_tc_fail("too many references to buf");
    }

    /*
     * Clean up buffer.
     */
    if (!buffer_dereference(&buf, MDL)) {
        atf_tc_fail("buffer_reference() failed");
    }
}

ATF_TC(data_string_forget_nobuf);

ATF_TC_HEAD(data_string_forget_nobuf, tc) {
    atf_tc_set_md_var(tc, "descr", "data_string_forget test, "
                      "data_string without buffer");
}

ATF_TC_BODY(data_string_forget_nobuf, tc) {
    struct data_string a;
    const char *str = "Lorem ipsum dolor sit amet massa nunc.";

    /*
     * Create the string we want to forget.
     */
    memset(&a, 0, sizeof(a));
    a.len = strlen(str);
    a.data = (const unsigned char *)str;
    a.terminated = 1;

    /*
     * Forget and confirm we've forgotten.
     */
    data_string_forget(&a, MDL);

    if (a.len != 0) {
        atf_tc_fail("incorrect length");
    }
    if (a.data != NULL) {
        atf_tc_fail("incorrect data");
    }
    if (a.terminated) {
        atf_tc_fail("incorrect terminated");
    }
    if (a.buffer != NULL) {
        atf_tc_fail("incorrect buffer");
    }
}

ATF_TC(data_string_copy);

ATF_TC_HEAD(data_string_copy, tc) {
    atf_tc_set_md_var(tc, "descr", "data_string_copy basic test");
}

ATF_TC_BODY(data_string_copy, tc) {
    struct data_string a, b;
    const char *str = "Lorem ipsum dolor sit amet orci aliquam.";

    /*
     * Create the string we want to copy.
         */
    memset(&a, 0, sizeof(a));
    a.len = strlen(str);
    if (!buffer_allocate(&a.buffer, a.len, MDL)) {
        atf_tc_fail("out of memory");
    }
    a.data = a.buffer->data;
    memcpy(a.buffer->data, str, a.len);

    /*
     * Copy the string, and confirm it works.
     */
    memset(&b, 0, sizeof(b));
    data_string_copy(&b, &a, MDL);

    if (b.len != a.len) {
        atf_tc_fail("incorrect length");
    }
    if (b.data != a.data) {
        atf_tc_fail("incorrect data");
    }
    if (b.terminated != a.terminated) {
        atf_tc_fail("incorrect terminated");
    }
    if (b.buffer != a.buffer) {
        atf_tc_fail("incorrect buffer");
    }

    /*
     * Clean up.
     */
    data_string_forget(&b, MDL);
    data_string_forget(&a, MDL);
}

ATF_TC(data_string_copy_nobuf);

ATF_TC_HEAD(data_string_copy_nobuf, tc) {
    atf_tc_set_md_var(tc, "descr", "data_string_copy test, "
                      "data_string without buffer");
}

ATF_TC_BODY(data_string_copy_nobuf, tc) {
    struct data_string a, b;
    const char *str = "Lorem ipsum dolor sit amet cras amet.";

    /*
     * Create the string we want to copy.
     */
    memset(&a, 0, sizeof(a));
    a.len = strlen(str);
    a.data = (const unsigned char *)str;
    a.terminated = 1;

    /*
     * Copy the string, and confirm it works.
     */
    memset(&b, 0, sizeof(b));
    data_string_copy(&b, &a, MDL);

    if (b.len != a.len) {
        atf_tc_fail("incorrect length");
    }
    if (b.data != a.data) {
        atf_tc_fail("incorrect data");
    }
    if (b.terminated != a.terminated) {
        atf_tc_fail("incorrect terminated");
    }
    if (b.buffer != a.buffer) {
        atf_tc_fail("incorrect buffer");
    }

    /*
     * Clean up.
     */
    data_string_forget(&b, MDL);
    data_string_forget(&a, MDL);

}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, buffer_allocate);
    ATF_TP_ADD_TC(tp, buffer_reference);
    ATF_TP_ADD_TC(tp, buffer_dereference);
    ATF_TP_ADD_TC(tp, data_string_forget);
    ATF_TP_ADD_TC(tp, data_string_forget_nobuf);
    ATF_TP_ADD_TC(tp, data_string_copy);
    ATF_TP_ADD_TC(tp, data_string_copy_nobuf);

    return (atf_no_error());
}

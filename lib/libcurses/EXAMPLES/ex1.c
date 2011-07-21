/*	$NetBSD: ex1.c,v 1.5 2007/05/28 15:01:58 blymn Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ex1.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */
#include <sys/types.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <assert.h>

#define YSIZE 4
#define XSIZE 30

void quit( int );

main()
{
	int i, j, c, n = 0, ac = 0;
	size_t len;
	char id[100];
	FILE *fp;
	char *s;
    cchar_t cc, ncc, cstr[ 128 ], icc, icstr[ 128 ], bcc;
    int wc_on = 0, wgc_on = 0;
    char mbs[] = "大";
    char mbstr[] = "大学之道，在明明德，在亲民，在止于至善。 (Liji)";
    wchar_t wstr[ 128 ], owstr[ 4 ], gwstr[ 128 ], iwstr[ 128 ];
    int wslen = 0;
    wint_t wc;
    char nostr[ 128 ];
    attr_t al[ 15 ] = { WA_BLINK,
                        WA_BOLD,
                        WA_DIM,
                        WA_LOW,
                        WA_TOP,
                        WA_INVIS,
                        WA_HORIZONTAL,
                        WA_VERTICAL,
                        WA_LEFT,
                        WA_RIGHT,
                        WA_PROTECT,
                        WA_REVERSE,
                        WA_STANDOUT,
                        WA_UNDERLINE };

    fprintf( stderr, "Current locale: %s\n", setlocale(LC_ALL, ""));
    if (( wslen =  mbstowcs( &cc.vals[ 0 ], mbs, strlen( mbs ))) < 0 ) {
        fprintf( stderr, "mbstowcs() failed\n" );
        return -1;
    }
    fprintf( stderr, "WC string length: %d\n", wslen );
    fprintf( stderr, "WC width: %d\n", wcwidth( cc.vals[ 0 ]));
    cc.elements = ncc.elements = 8;
    cc.attributes = ncc.attributes = 0;
    ncc.vals[ 0 ] = 0xda00;
    cc.vals[ 1 ] = ncc.vals[ 1 ] = 0xda01;
    cc.vals[ 2 ] = ncc.vals[ 2 ] = 0xda02;
    cc.vals[ 3 ] = ncc.vals[ 3 ] = 0xda03;
    cc.vals[ 4 ] = ncc.vals[ 4 ] = 0xda04;
    cc.vals[ 5 ] = ncc.vals[ 5 ] = 0xda05;
    cc.vals[ 6 ] = ncc.vals[ 6 ] = 0xda06;
    cc.vals[ 7 ] = ncc.vals[ 7 ] = 0xda07;

    if (( wslen =  mbstowcs( wstr, mbstr, strlen( mbstr ))) < 0 ) {
        fprintf( stderr, "mbstowcs() failed\n" );
        return -1;
    }

    for ( i = 0; i < wslen; i++ ) {
        cstr[ i ].vals[ 0 ] = wstr[ i ];
    }
    cstr[ wslen ].vals[ 0 ] = 0;

    bcc.elements = 8;
    bcc.attributes = 0;
    bcc.vals[ 0 ] = L'_';
    bcc.vals[ 1 ] = 0xda01;
    bcc.vals[ 2 ] = 0xda02;
    bcc.vals[ 3 ] = 0xda03;
    bcc.vals[ 4 ] = 0xda04;
    bcc.vals[ 5 ] = 0xda05;
    bcc.vals[ 6 ] = 0xda06;
    bcc.vals[ 7 ] = 0xda07;

	initscr();			/* Always call initscr() first */
	signal(SIGINT, quit);		/* Make sure wou have a 'cleanup' fn */
	crmode();			/* We want cbreak mode */
	noecho();			/* We want to have control of chars */
	delwin(stdscr);			/* Create our own stdscr */
	stdscr = newwin(YSIZE, XSIZE, 1, 1);
	flushok(stdscr, TRUE);		/* Enable flushing of stdout */
	scrollok(stdscr, TRUE);		/* Enable scrolling */
	erase();			/* Initially, clear the screen */

	standout();
	move(0,0);
	while (1) {
        if ( !wgc_on ) {
		    c = getchar();
		    switch(c) {
		        case 'q':		/* Quit on 'q' */
			        quit( 0 );
			        break;
                case 'p':
                    keypad( stdscr, TRUE );
                    break;
                case 'P':
                    keypad( stdscr, FALSE );
                    break;
                case 'g':
                    wgc_on = 1;
                    echo();
                    break;
                case 'b':
                    get_wstr( gwstr );
                    move( 1, 0 );
                    addstr( "Input:" );
                    addwstr( gwstr );
                    refresh();
                    break;
                case 'h':
                    move( 0, 0 );
                    in_wch( &icc );
                    move( 1, 0 );
                    add_wch( &icc );
                    refresh();
                    break;
                case 'y':
                    move( 0, 0 );
                    in_wchstr( icstr );
                    move( 1, 0 );
                    add_wchstr( icstr );
                    refresh();
                    break;
                case 'u':
                    move( 0, 0 );
                    inwstr( iwstr );
                    move( 1, 0 );
                    addwstr( iwstr );
                    refresh();
                    break;
                case 'i':
                    move( 0, 0 );
                    hline_set( &cc, 20 );
                    move( 0, 0 );
                    vline_set( &cc, 20 );
                    refresh();
                    break;
                case 'o':
                    clrtobot();
                    refresh();
                    break;
		        case 's':		/* Go into standout mode on 's' */
			        standout();
			        break;
		        case 'e':		/* Exit standout mode on 'e' */
			        standend();
			        break;
		        case 'r':		/* Force a refresh on 'r' */
			        wrefresh(curscr);
			        break;
		        case 'w':		/* Turn on/off add_wch() tests */
                    wc_on = 1 - wc_on;
                    break;
                case 'd':
                    add_wchstr(( const cchar_t *)&cstr );
                    refresh();
                    break;
                case 'c':
                    addwstr(( const wchar_t *)&wstr );
                    refresh();
                    break;
                case 'z':
                    move( 0, 1 );
                    if ( wc_on )
                        add_wch( &cc );
                    else
                        addch( c );
                    refresh();
                    break;
                case 'x':
                    move( 0, 3 );
                    if ( wc_on )
                        add_wch( &cc );
                    else
                        addch( c );
                    refresh();
                    break;
                case 'n':
                    add_wch( &ncc );
                    refresh();
                    break;
                case 'm':
                    //border( 0, 0, 0, 0, 0, 0, 0, 0 );
                    border_set( NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL );
                    refresh();
                    break;
                case 'j':
                    box_set( stdscr, &cc, &cc );
                    refresh();
                    break;
                case 'k':
                    erase();
                    refresh();
                    break;
                case '1':
                    move( 0, 0 );
                    clrtoeol();
                    refresh();
                    break;
                case '2':
                    move( 0, 0 );
                    sprintf( nostr, "Orig:%x", al[ ac ]);
                    addstr( nostr );
                    ac = ( ac + 1 ) % 16;
                    bcc.attributes = al[ ac ];
                    bkgrnd( &bcc );
                    move( 1, 0 );
                    sprintf( nostr, "New:%x", al[ ac ]);
                    //addstr( nostr );
                    insstr( nostr );
                    refresh();
                    break;
                case 'v':
                    if ( wc_on )
                        ins_wch( &cc );
                    else
                        insch( c );
                    refresh();
                    break;
                case 'f':
                    ins_wstr(( const wchar_t *)&wstr );
                    refresh();
                    break;
                case 't':
                    for ( i = 0; i < 4; i++ ) {
                        owstr[ i ] = wstr[ i + 5 ];
                        wstr[ i + 5 ] = i + 0xda05;
                    }
                    ins_wstr(( const wchar_t *)&wstr );
                    refresh();
                    for ( i = 0; i < 4; i++ )
                        wstr[ i + 5 ] = owstr[ i ];
                    break;
		        default:		/* By default output the character */
                    if ( wc_on )
                        add_wch( &cc );
                    else {
                        if ( c < 0x7F )
                            addch( c );
                        else {
                            addstr( keyname( c ));
                        }
                    }
                    refresh();
		    }
        } else {
            get_wch( &wc );
            switch ( wc ) {
                case L'w':
                    wgc_on = 0;
                    noecho();
                    break;
                case L'q':
                    quit( 0 );
                    break;
                case L't':
                    notimeout( stdscr, TRUE );
                    break;
                case L'T':
                    notimeout( stdscr, FALSE );
                    break;
                case L'd':
                    nodelay( stdscr, TRUE );
                    break;
                case L'D':
                    nodelay( stdscr, FALSE );
                    break;
                case L'p':
                    keypad( stdscr, TRUE );
                    break;
                case L'P':
                    keypad( stdscr, FALSE );
                    break;
                default:
                    break;
            }
        }
	}
}

void quit( int sig )
{
	erase();		/* Terminate by erasing the screen */
	refresh();
	endwin();		/* Always end with endwin() */
	delwin(curscr);		/* Return storage */
	delwin(stdscr);
	putchar('\n');
	exit( sig );
}


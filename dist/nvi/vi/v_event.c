/*	$NetBSD: v_event.c,v 1.3 2009/01/18 03:45:50 lukem Exp $ */

/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: v_event.c,v 8.21 2001/06/25 15:19:31 skimo Exp (Berkeley) Date: 2001/06/25 15:19:31";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ipc/ip.h"
#include "vi.h"

/*
 * v_c_settop --
 *	Scrollbar position.
 */
static int
v_c_settop(SCR *sp, VICMD *vp)
{
	SMAP *smp;
	size_t x = 0, y = LASTLINE(sp); /* Future: change to -1 to not
					 * display the cursor
					 */
	size_t tx, ty = -1;

	/*
	 * We want to scroll the screen, without changing the cursor position.
	 * So, we fill the screen map and then flush it to the screen.  Then,
	 * set the VIP_S_REFRESH flag so the main vi loop doesn't update the
	 * screen.  When the next real command happens, the refresh code will
	 * notice that the screen map is way wrong and fix it.
	 *
	 * XXX
	 * There may be a serious performance problem here -- we're doing no
	 * optimization whatsoever, which means that we're copying the entire
	 * screen out to the X11 screen code on each change.
	 */
	if (vs_sm_fill(sp, vp->ev.e_lno, P_TOP))
		return (1);
	for (smp = HMAP; smp <= TMAP; ++smp) {
                SMAP_FLUSH(smp);
		if (vs_line(sp, smp, &ty, &tx))
			return (1);
		if (ty != (size_t)-1) {
			y = ty;
			x = tx;
		}
        }
	(void)sp->gp->scr_move(sp, y, x);

	F_SET(VIP(sp), VIP_S_REFRESH);

	return (sp->gp->scr_refresh(sp, 0));
}

/*
 * v_edit --
 *	Edit command.
 */
static int
v_edit(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_editopt --
 *	Set an option value.
 */
static int
v_editopt(SCR *sp, VICMD *vp)
{
	int rval;
	const char *np;
	size_t nlen;
	char *p2;

	INT2CHAR(sp, vp->ev.e_str2, STRLEN(vp->ev.e_str2)+1, np, nlen);
	p2 = strdup(np);
	rval = api_opts_set(sp, vp->ev.e_str1, p2, 
			    vp->ev.e_val1, vp->ev.e_val1);
	if (sp->gp->scr_reply != NULL)
		(void)sp->gp->scr_reply(sp, rval, NULL);
	free(p2);
	return (rval);
}

/*
 * v_editsplit --
 *	Edit in a split screen.
 */
static int
v_editsplit(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0);
	F_SET(&cmd, E_NEWSCREEN);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_tag --
 *	Tag command.
 */
static int
v_tag(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	if (v_curword(sp))
		return (1);

	ex_cinit(sp, &cmd, C_TAG, 0, OOBLNO, OOBLNO, 0);
	argv_exp0(sp, &cmd, VIP(sp)->keyw, STRLEN(VIP(sp)->keyw));
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_tagas --
 *	Tag on the supplied string.
 */
static int
v_tagas(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_TAG, 0, OOBLNO, OOBLNO, 0);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_tagsplit --
 *	Tag in a split screen.
 */
static int
v_tagsplit(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	if (v_curword(sp))
		return (1);

	ex_cinit(sp, &cmd, C_TAG, 0, OOBLNO, OOBLNO, 0);
	F_SET(&cmd, E_NEWSCREEN);
	argv_exp0(sp, &cmd, VIP(sp)->keyw, STRLEN(VIP(sp)->keyw));
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_quit --
 *	Quit command.
 */
static int
v_quit(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_QUIT, 0, OOBLNO, OOBLNO, 0);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_erepaint --
 *	Repaint selected lines from the screen.
 *
 * PUBLIC: int v_erepaint __P((SCR *, EVENT *));
 */
int
v_erepaint(SCR *sp, EVENT *evp)
{
	SMAP *smp;

	for (; evp->e_flno <= evp->e_tlno; ++evp->e_flno) {
		smp = HMAP + evp->e_flno - 1;
		SMAP_FLUSH(smp);
		if (vs_line(sp, smp, NULL, NULL))
			return (1);
	}
	return (0);
}

/*
 * v_sel_end --
 *	End selection.
 */
static int
v_sel_end(SCR *sp, EVENT *evp)
{
	SMAP *smp;
	VI_PRIVATE *vip;

	smp = HMAP + evp->e_lno;
	if (smp > TMAP) {
		/* XXX */
		return (1);
	}

	vip = VIP(sp);
	vip->sel.lno = smp->lno;
	vip->sel.cno =
	    vs_colpos(sp, smp->lno, evp->e_cno + (smp->soff - 1) * sp->cols);
	return (0);
}

/*
 * v_sel_start --
 *	Start selection.
 */
static int
v_sel_start(SCR *sp, EVENT *evp)
{
	SMAP *smp;
	VI_PRIVATE *vip;

	smp = HMAP + evp->e_lno;
	if (smp > TMAP) {
		/* XXX */
		return (1);
	}

	vip = VIP(sp);
	vip->sel.lno = smp->lno;
	vip->sel.cno =
	    vs_colpos(sp, smp->lno, evp->e_cno + (smp->soff - 1) * sp->cols);
	return (0);
}

/*
 * v_wq --
 *	Write and quit command.
 */
static int
v_wq(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_WQ, 0, OOBLNO, OOBLNO, 0);

	cmd.addr1.lno = 1;
	if (db_last(sp, &cmd.addr2.lno))
		return (1);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_write --
 *	Write command.
 */
static int
v_write(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_WRITE, 0, OOBLNO, OOBLNO, 0);

	cmd.addr1.lno = 1;
	if (db_last(sp, &cmd.addr2.lno))
		return (1);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_writeas --
 *	Write command.
 */
static int
v_writeas(SCR *sp, VICMD *vp)
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_WRITE, 0, OOBLNO, OOBLNO, 0);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);

	cmd.addr1.lno = 1;
	if (db_last(sp, &cmd.addr2.lno))
		return (1);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_event --
 *	Find the event associated with a function.
 *
 * PUBLIC: int v_event __P((SCR *, VICMD *));
 */
int
v_event(SCR *sp, VICMD *vp)
{
	/* This array maps events to vi command functions. */
#define VIKEYDEF(a, b) { a, b, NULL, NULL }
	static VIKEYS const vievents[] = {
#define	V_C_SETTOP	 0				/* VI_C_SETTOP */
		VIKEYDEF(v_c_settop,	0),
#define	V_EDIT		 1				/* VI_EDIT */
		VIKEYDEF(v_edit,	0),
#define	V_EDITOPT	 2				/* VI_EDITOPT */
		VIKEYDEF(v_editopt,	0),
#define	V_EDITSPLIT	 3				/* VI_EDITSPLIT */
		VIKEYDEF(v_editsplit,	0),
#define	V_EMARK		 4				/* VI_MOUSE_MOVE */
		VIKEYDEF(v_emark,	V_ABS_L|V_MOVE),
#define	V_QUIT		 5				/* VI_QUIT */
		VIKEYDEF(v_quit,	0),
#define	V_SEARCH	 6				/* VI_SEARCH */
		VIKEYDEF(v_esearch,	V_ABS_L|V_MOVE),
#define	V_TAG		 7				/* VI_TAG */
		VIKEYDEF(v_tag,	0),
#define	V_TAGAS	 8					/* VI_TAGAS */
		VIKEYDEF(v_tagas,	0),
#define	V_TAGSPLIT	 9				/* VI_TAGSPLIT */
		VIKEYDEF(v_tagsplit,	0),
#define	V_WQ		10				/* VI_WQ */
		VIKEYDEF(v_wq,		0),
#define	V_WRITE	11					/* VI_WRITE */
		VIKEYDEF(v_write,	0),
#define	V_WRITEAS	12				/* VI_WRITEAS */
		VIKEYDEF(v_writeas,	0),
	};

	switch (vp->ev.e_ipcom) {
	case VI_C_BOL:
		vp->kp = &vikeys['0'];
		break;
	case VI_C_BOTTOM:
		vp->kp = &vikeys['G'];
		break;
	case VI_C_DEL:
		vp->kp = &vikeys['x'];
		break;
	case VI_C_DOWN:
		F_SET(vp, VC_C1SET);
		vp->count = vp->ev.e_lno;
		vp->kp = &vikeys['\012'];
		break;
	case VI_C_EOL:
		vp->kp = &vikeys['$'];
		break;
	case VI_C_INSERT:
		vp->kp = &vikeys['i'];
		break;
	case VI_C_LEFT:
		vp->kp = &vikeys['\010'];
		break;
	case VI_C_PGDOWN:
		F_SET(vp, VC_C1SET);
		vp->count = vp->ev.e_lno;
		vp->kp = &vikeys['\006'];
		break;
	case VI_C_PGUP:
		F_SET(vp, VC_C1SET);
		vp->count = vp->ev.e_lno;
		vp->kp = &vikeys['\002'];
		break;
	case VI_C_RIGHT:
		vp->kp = &vikeys['\040'];
		break;
	case VI_C_SEARCH:
		vp->kp = &vievents[V_SEARCH];
		break;
	case VI_C_SETTOP:
		vp->kp = &vievents[V_C_SETTOP];
		break;
	case VI_C_TOP:
		F_SET(vp, VC_C1SET);
		vp->count = 1;
		vp->kp = &vikeys['G'];
		break;
	case VI_C_UP:
		F_SET(vp, VC_C1SET);
		vp->count = vp->ev.e_lno;
		vp->kp = &vikeys['\020'];
		break;
	case VI_EDIT:
		vp->kp = &vievents[V_EDIT];
		break;
	case VI_EDITOPT:
		vp->kp = &vievents[V_EDITOPT];
		break;
	case VI_EDITSPLIT:
		vp->kp = &vievents[V_EDITSPLIT];
		break;
	case VI_MOUSE_MOVE:
		vp->kp = &vievents[V_EMARK];
		break;
	case VI_SEL_END:
		v_sel_end(sp, &vp->ev);
		/* XXX RETURN IGNORE */
		break;
	case VI_SEL_START:
		v_sel_start(sp, &vp->ev);
		/* XXX RETURN IGNORE */
		break;
	case VI_QUIT:
		vp->kp = &vievents[V_QUIT];
		break;
	case VI_TAG:
		vp->kp = &vievents[V_TAG];
		break;
	case VI_TAGAS:
		vp->kp = &vievents[V_TAGAS];
		break;
	case VI_TAGSPLIT:
		vp->kp = &vievents[V_TAGSPLIT];
		break;
	case VI_UNDO:
		vp->kp = &vikeys['u'];
		break;
	case VI_WQ:
		vp->kp = &vievents[V_WQ];
		break;
	case VI_WRITE:
		vp->kp = &vievents[V_WRITE];
		break;
	case VI_WRITEAS:
		vp->kp = &vievents[V_WRITEAS];
		break;
	default:
		return (1);
	}
	return (0);
}

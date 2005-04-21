(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE RealInOut;
(*
  Module:       InOut for REAL numbers
  Author:	Ceriel J.H. Jacobs
  Version:      $Header$
*)

  FROM	InOut IMPORT	ReadString, WriteString, WriteOct;
  FROM	Traps IMPORT	Message;
  FROM	SYSTEM IMPORT	WORD;
  FROM	RealConversions IMPORT
			LongRealToString, StringToLongReal;

  CONST	MAXNDIG = 32;
	MAXWIDTH = MAXNDIG+7;
  TYPE	RBUF = ARRAY [0..MAXWIDTH+1] OF CHAR;

  PROCEDURE WriteReal(arg: REAL; ndigits: CARDINAL);
  BEGIN
	WriteLongReal(LONG(arg), ndigits)
  END WriteReal;

  PROCEDURE WriteLongReal(arg: LONGREAL; ndigits: CARDINAL);
    VAR buf : RBUF;
	ok : BOOLEAN;

  BEGIN
	IF ndigits > MAXWIDTH THEN ndigits := MAXWIDTH; END;
	IF ndigits < 10 THEN ndigits := 10; END;
	LongRealToString(arg, ndigits, -INTEGER(ndigits - 7), buf, ok);
	WriteString(buf);
  END WriteLongReal;

  PROCEDURE WriteFixPt(arg: REAL; n, k: CARDINAL);
  BEGIN
	WriteLongFixPt(LONG(arg), n, k)
  END WriteFixPt;

  PROCEDURE WriteLongFixPt(arg: LONGREAL; n, k: CARDINAL);
  VAR buf: RBUF;
      ok : BOOLEAN;

  BEGIN
	IF n > MAXWIDTH THEN n := MAXWIDTH END;
	LongRealToString(arg, n, k, buf, ok);
	WriteString(buf);
  END WriteLongFixPt;

  PROCEDURE ReadReal(VAR x: REAL);
  VAR x1: LONGREAL;
  BEGIN
	ReadLongReal(x1);
	x := x1
  END ReadReal;

  PROCEDURE ReadLongReal(VAR x: LONGREAL);
    VAR	Buf: ARRAY[0..512] OF CHAR;
	ok: BOOLEAN;

  BEGIN
	ReadString(Buf);
	StringToLongReal(Buf, x, ok);
	IF NOT ok THEN
		Message("real expected");
		HALT;
	END;
	Done := TRUE;
  END ReadLongReal;

  PROCEDURE wroct(x: ARRAY OF WORD);
  VAR	i: CARDINAL;
  BEGIN
	FOR i := 0 TO HIGH(x) DO
		WriteOct(CARDINAL(x[i]), 0);
		WriteString("  ");
	END;
  END wroct;

  PROCEDURE WriteRealOct(x: REAL);
  BEGIN
	wroct(x);
  END WriteRealOct;

  PROCEDURE WriteLongRealOct(x: LONGREAL);
  BEGIN
	wroct(x);
  END WriteLongRealOct;

BEGIN
	Done := FALSE;
END RealInOut.

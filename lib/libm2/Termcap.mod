(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)
 
(* 
  Module:       Interface to termcap database
  From:         Unix manual chapter 3 
  Version:      $Header$ 
*)

(*$R-*)
IMPLEMENTATION MODULE Termcap;

  IMPORT XXTermcap;
  FROM	SYSTEM IMPORT	ADR, ADDRESS;
  FROM	Unix IMPORT	isatty;
  FROM	Arguments IMPORT
			GetEnv;

  TYPE	STR = ARRAY[1..32] OF CHAR;
	STRCAP = POINTER TO STR;

  VAR	Buf, Buf1 : ARRAY [1..1024] OF CHAR;
	BufCnt : INTEGER;

  PROCEDURE Tgetent(name: ARRAY OF CHAR) : INTEGER;
  VAR i: INTEGER;
      x: STRCAP;
  BEGIN
	i := XXTermcap.tgetent(ADR(Buf), ADR(name));
	BufCnt := 1;
	IF isatty(1) THEN
	ELSE
		(* This used to be something returned by gtty().  To increase
		 * portability we forget about old terminals needing delays.
		 * (kjb)
		 *)
		XXTermcap.ospeed := 0;
	END;
	IF i > 0 THEN
		IF Tgetstr("pc", x) THEN
			XXTermcap.PC := x^[1];
		ELSE	XXTermcap.PC := 0C;
		END;
		IF Tgetstr("up", x) THEN ; END; XXTermcap.UP := x;
		IF Tgetstr("bc", x) THEN ; END; XXTermcap.BC := x;
	END;
	RETURN i;
  END Tgetent;

  PROCEDURE Tgetnum(id: ARRAY OF CHAR): INTEGER;
  BEGIN
	RETURN XXTermcap.tgetnum(ADR(id));
  END Tgetnum;

  PROCEDURE Tgetflag(id: ARRAY OF CHAR): BOOLEAN;
  BEGIN
	RETURN XXTermcap.tgetflag(ADR(id)) = 1;
  END Tgetflag;

  PROCEDURE Tgoto(cm: STRCAP; col, line: INTEGER): STRCAP;
  BEGIN
	RETURN XXTermcap.tgoto(cm, col, line);
  END Tgoto;

  PROCEDURE Tgetstr(id: ARRAY OF CHAR; VAR res: STRCAP) : BOOLEAN;
  VAR a, a2: ADDRESS;
      b: CARDINAL;
  BEGIN
	a := ADR(Buf1[BufCnt]);
	a2 := XXTermcap.tgetstr(ADR(id), ADR(a));
	res := a2;
	IF a2 = NIL THEN
		RETURN FALSE;
	END;
	b := a - a2;
	INC(BufCnt, b);
	RETURN TRUE;
  END Tgetstr;

  PROCEDURE Tputs(cp: STRCAP; affcnt: INTEGER; p: PUTPROC);
  BEGIN
	XXTermcap.tputs(cp, affcnt, XXTermcap.PUTPROC(p));
  END Tputs;

  PROCEDURE InitTermcap;
  VAR Bf: STR;
  BEGIN
	IF GetEnv("TERM", Bf) = 0 THEN
		Bf := "dumb";
	END;
	IF Tgetent(Bf) <= 0 THEN
	END;
  END InitTermcap;

BEGIN
	InitTermcap;
END Termcap.

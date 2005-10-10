(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE Traps;
(*
  Module:       Facility for handling traps
  Author:       Ceriel J.H. Jacobs
  Version:      $Header$
*)

  FROM	EM IMPORT	SIG, LINO, FILN, TRP;
  FROM	Unix IMPORT	write;
  FROM	SYSTEM IMPORT	ADDRESS, ADR;
  FROM	Arguments IMPORT
			Argv;

  PROCEDURE InstallTrapHandler(t: TrapHandler): TrapHandler;
  (* Install a new trap handler, and return the previous one.
     Parameter of trap handler is the trap number.
  *)
  BEGIN
	RETURN SIG(t);
  END InstallTrapHandler;

  PROCEDURE Message(str: ARRAY OF CHAR);
  (* Write message "str" on standard error, preceeded by filename and
     linenumber if possible
  *)
  VAR 	p: POINTER TO CHAR;
	l: CARDINAL;
	lino: INTEGER;
	buf, buf2: ARRAY [0..255] OF CHAR;
	i, j: CARDINAL;
  BEGIN
	p := FILN();
	IF p # NIL THEN
		i := 1;
		buf[0] := '"';
		WHILE p^ # 0C DO
			buf[i] := p^;
			INC(i);
			p := ADDRESS(p) + 1;
		END;
		buf[i] := '"';
		INC(i);
		IF write(2, ADR(buf), i) < 0 THEN END;
	ELSE
		l := Argv(0, buf);
		IF write(2, ADR(buf), l-1) < 0 THEN END;
	END;
	lino := LINO();
	i := 0;
	IF lino # 0 THEN
		i := 7;
		buf[0] := ','; buf[1] := ' ';
		buf[2] := 'l'; buf[3] := 'i'; buf[4] := 'n'; buf[5] := 'e';
		buf[6] := ' ';
		IF lino < 0 THEN
			buf[7] := '-';
			i := 8;
			lino := - lino;
		END;
		j := 0;
		REPEAT
			buf2[j] := CHR(CARDINAL(lino) MOD 10 + ORD('0'));
			lino := lino DIV 10;
			INC(j);
		UNTIL lino = 0;
		WHILE j > 0 DO
			DEC(j);
			buf[i] := buf2[j];
			INC(i);
		END;
	END;
	buf[i] := ':';
	buf[i+1] := ' ';
	IF write(2, ADR(buf), i+2) < 0 THEN END;
	i := 0;
	WHILE (i <= HIGH(str)) AND (str[i] # 0C) DO
		INC(i);
	END;
	IF write(2, ADR(str), i) < 0 THEN END;
	buf[0] := 12C;
	IF write(2, ADR(buf), 1) < 0 THEN END;
  END Message;

  PROCEDURE Trap(n: INTEGER);
  (* cause trap number "n" to occur *)
  BEGIN
	TRP(n);
  END Trap;

END Traps.

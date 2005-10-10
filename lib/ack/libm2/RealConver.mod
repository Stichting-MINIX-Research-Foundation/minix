(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE RealConversions;
(*
  Module:       string-to-real and real-to-string conversions
  Author:       Ceriel J.H. Jacobs
  Version:      $Header$
*)


  PROCEDURE RealToString(arg: REAL;
		width, digits: INTEGER;
		VAR str: ARRAY OF CHAR;
		VAR ok: BOOLEAN);
  BEGIN
	LongRealToString(LONG(arg), width, digits, str, ok);
  END RealToString;

  TYPE
	Powers = RECORD
		pval: LONGREAL;
		rpval: LONGREAL;
		exp: INTEGER
	END;

  VAR Powers10: ARRAY[1..6] OF Powers;

  PROCEDURE LongRealToString(arg: LONGREAL;
		width, digits: INTEGER;
		VAR str: ARRAY OF CHAR;
		VAR ok: BOOLEAN);
    VAR	pointpos: INTEGER;
	i: CARDINAL;
	ecvtflag: BOOLEAN;
	r: LONGREAL;
	ind1, ind2 : CARDINAL;
	sign: BOOLEAN;
	ndigits: CARDINAL;

  BEGIN
	r := arg;
	IF digits < 0 THEN
		ecvtflag := TRUE;
		ndigits := -digits;
	ELSE
		ecvtflag := FALSE;
		ndigits := digits;
	END;
	IF (HIGH(str) < ndigits + 3) THEN
		str[0] := 0C; ok := FALSE; RETURN
	END;
	pointpos := 0;
	sign := r < 0.0D;
	IF sign THEN r := -r END;
	ok := TRUE;
	IF NOT (r / 10.0D < r) THEN
		(* assume Nan or Infinity *)
		r := 0.0D;
		ok := FALSE;
	END;
	IF r # 0.0D THEN
		IF r >= 10.0D THEN
			FOR i := 1 TO 6 DO
				WITH Powers10[i] DO
					WHILE r >= pval DO
						r := r * rpval;
						INC(pointpos, exp)
					END;
				END;
			END;
		END;
		IF r < 1.0D THEN
			FOR i := 1 TO 6 DO
				WITH Powers10[i] DO
					WHILE r*pval < 10.0D DO
						r := r * pval;
						DEC(pointpos, exp)
					END;
				END;
			END;
		END;
		(* Now, we have r in [1.0, 10.0) *)
		INC(pointpos);
	END;
	ind1 := 0;
	ind2 := ndigits+1;

	IF NOT ecvtflag THEN 
		IF INTEGER(ind2) + pointpos <= 0 THEN
			ind2 := 1;
		ELSE
			ind2 := INTEGER(ind2) + pointpos
		END;
	END;
	IF ind2 > HIGH(str) THEN
		ok := FALSE;
		str[0] := 0C;
		RETURN;
	END;
	WHILE ind1 < ind2 DO
		str[ind1] := CHR(TRUNC(r)+ORD('0'));
		r := 10.0D * (r - FLOATD(TRUNC(r)));
		INC(ind1);
	END;
	IF ind2 > 0 THEN
		DEC(ind2);
		ind1 := ind2;
		str[ind2] := CHR(ORD(str[ind2])+5);
		WHILE str[ind2] > '9' DO
			str[ind2] := '0';
			IF ind2 > 0 THEN
				DEC(ind2);
				str[ind2] := CHR(ORD(str[ind2])+1);
			ELSE
				str[ind2] := '1';
				INC(pointpos);
				IF NOT ecvtflag THEN
					IF ind1 > 0 THEN str[ind1] := '0'; END;
					INC(ind1);
				END;
			END;
		END;
		IF (NOT ecvtflag) AND (ind1 = 0) THEN
			str[0] := CHR(ORD(str[0])-5);
			INC(ind1);
		END;
	END;
	IF ecvtflag THEN
		FOR i := ind1 TO 2 BY -1 DO
			str[i] := str[i-1];
		END;
		str[1] := '.';
		INC(ind1);
		IF sign THEN
			FOR i := ind1 TO 1 BY -1 DO
				str[i] := str[i-1];
			END;
			INC(ind1);
			str[0] := '-';
		END;
		IF (ind1 + 4) > HIGH(str) THEN
			str[0] := 0C;
			ok := FALSE;
			RETURN;
		END;
		str[ind1] := 'E'; INC(ind1);
		IF arg # 0.0D THEN DEC(pointpos); END;
		IF pointpos < 0 THEN
			pointpos := -pointpos;
			str[ind1] := '-';
		ELSE
			str[ind1] := '+';
		END;
		INC(ind1);
		str[ind1] := CHR(ORD('0') + CARDINAL(pointpos DIV 100));
		pointpos := pointpos MOD 100;
		INC(ind1);
		str[ind1] := CHR(ORD('0') + CARDINAL(pointpos DIV 10));
		INC(ind1);
		str[ind1] := CHR(ORD('0') + CARDINAL(pointpos MOD 10));
	ELSE
		IF pointpos <= 0 THEN
			FOR i := ind1 TO 1 BY -1 DO
				str[i+CARDINAL(-pointpos)] := str[i-1];
			END;
			FOR i := 0 TO CARDINAL(-pointpos) DO
				str[i] := '0';
			END;
			ind1 := ind1 + CARDINAL(1 - pointpos);
			pointpos := 1;
		END;
		FOR i := ind1 TO CARDINAL(pointpos+1) BY -1 DO
			str[i] := str[i-1];
		END;
		IF ndigits = 0 THEN
			str[pointpos] := 0C;
			ind1 := pointpos - 1;
		ELSE
			str[pointpos] := '.';
			IF INTEGER(ind1) > pointpos+INTEGER(ndigits) THEN
				ind1 := pointpos+INTEGER(ndigits);
			END;
			str[pointpos+INTEGER(ndigits)+1] := 0C;
		END;
		IF sign THEN
			FOR i := ind1 TO 0 BY -1 DO
				str[i+1] := str[i];
			END;
			str[0] := '-';
			INC(ind1);
		END;
	END;
	IF (ind1+1) <= HIGH(str) THEN str[ind1+1] := 0C; END;
	IF ind1 >= CARDINAL(width) THEN
		ok := FALSE;
		RETURN;
	END;
	IF width > 0 THEN
		DEC(width);
	END;
	IF (width > 0) AND (ind1 < CARDINAL(width)) THEN
		FOR i := ind1 TO 0 BY -1 DO
			str[i + CARDINAL(width) - ind1] := str[i];
		END;
		FOR i := 0 TO CARDINAL(width)-(ind1+1) DO
			str[i] := ' ';
		END;
		ind1 := CARDINAL(width);
		IF (ind1+1) <= HIGH(str) THEN
			FOR ind1 := ind1+1 TO HIGH(str) DO
				str[ind1] := 0C;
			END;
		END;
	END;

  END LongRealToString;

	
  PROCEDURE StringToReal(str: ARRAY OF CHAR;
			 VAR r: REAL; VAR ok: BOOLEAN);
    VAR x: LONGREAL;
  BEGIN
	StringToLongReal(str, x, ok);
	IF ok THEN
		r := x;
	END;
  END StringToReal;

  PROCEDURE StringToLongReal(str: ARRAY OF CHAR;
			 VAR r: LONGREAL; VAR ok: BOOLEAN);
    CONST	BIG = 1.0D17;
    TYPE	SETOFCHAR = SET OF CHAR;
    VAR		pow10 : INTEGER;
		i : INTEGER;
		e : LONGREAL;
		ch : CHAR;
		signed: BOOLEAN;
		signedexp: BOOLEAN;
		iB: CARDINAL;

  BEGIN
	r := 0.0D;
	pow10 := 0;
	iB := 0;
	ok := TRUE;
	signed := FALSE;
	WHILE (str[iB] = ' ') OR (str[iB] = CHR(9)) DO
		INC(iB);
		IF iB > HIGH(str) THEN
			ok := FALSE;
			RETURN;
		END;
	END;
	IF str[iB] = '-' THEN signed := TRUE; INC(iB)
	ELSIF str[iB] = '+' THEN INC(iB)
	END;
	ch := str[iB]; INC(iB);
	IF NOT (ch IN SETOFCHAR{'0'..'9'}) THEN ok := FALSE; RETURN END;
	REPEAT
		IF r>BIG THEN INC(pow10) ELSE r:= 10.0D*r+FLOATD(ORD(ch)-ORD('0')) END;
		IF iB <= HIGH(str) THEN
			ch := str[iB]; INC(iB);
		END;
	UNTIL (iB > HIGH(str)) OR NOT (ch IN SETOFCHAR{'0'..'9'});
	IF (ch = '.') AND (iB <= HIGH(str)) THEN
		ch := str[iB]; INC(iB);
		IF NOT (ch IN SETOFCHAR{'0'..'9'}) THEN ok := FALSE; RETURN END;
		REPEAT
			IF r < BIG THEN
				r := 10.0D * r + FLOATD(ORD(ch)-ORD('0'));
				DEC(pow10);
			END;
			IF iB <= HIGH(str) THEN
				ch := str[iB]; INC(iB);
			END;
		UNTIL (iB > HIGH(str)) OR NOT (ch IN SETOFCHAR{'0'..'9'});
	END;
	IF (ch = 'E') THEN
		IF iB > HIGH(str) THEN
			ok := FALSE;
			RETURN;
		ELSE
			ch := str[iB]; INC(iB);
		END;
		i := 0;
		signedexp := FALSE;
		IF (ch = '-') OR (ch = '+') THEN
			signedexp := ch = '-';
			IF iB > HIGH(str) THEN
				ok := FALSE;
				RETURN;
			ELSE
				ch := str[iB]; INC(iB);
			END;
		END;
		IF NOT (ch IN SETOFCHAR{'0'..'9'}) THEN ok := FALSE; RETURN END;
		REPEAT
			i := i*10 + INTEGER(ORD(ch) - ORD('0'));
			IF iB <= HIGH(str) THEN
				ch := str[iB]; INC(iB);
			END;
		UNTIL (iB > HIGH(str)) OR NOT (ch IN SETOFCHAR{'0'..'9'});
		IF signedexp THEN i := -i END;
		pow10 := pow10 + i;
	END;
	IF pow10 < 0 THEN i := -pow10; ELSE i := pow10; END;
	e := 1.0D;
	DEC(i);
	WHILE i >= 10 DO
		e := e * 10000000000.0D;
		DEC(i,10);
	END;
	WHILE i >= 0 DO
		e := e * 10.0D;
		DEC(i)
	END;
	IF pow10<0 THEN
		r := r / e;
	ELSE
		r := r * e;
	END;
	IF signed THEN r := -r; END;
	IF (iB <= HIGH(str)) AND (ORD(ch) > ORD(' ')) THEN ok := FALSE; END
  END StringToLongReal;

BEGIN
	WITH Powers10[1] DO pval := 1.0D32; rpval := 1.0D-32; exp := 32 END;
	WITH Powers10[2] DO pval := 1.0D16; rpval := 1.0D-16; exp := 16 END;
	WITH Powers10[3] DO pval := 1.0D8; rpval := 1.0D-8; exp := 8 END;
	WITH Powers10[4] DO pval := 1.0D4; rpval := 1.0D-4; exp := 4 END;
	WITH Powers10[5] DO pval := 1.0D2; rpval := 1.0D-2; exp := 2 END;
	WITH Powers10[6] DO pval := 1.0D1; rpval := 1.0D-1; exp := 1 END;
END RealConversions.

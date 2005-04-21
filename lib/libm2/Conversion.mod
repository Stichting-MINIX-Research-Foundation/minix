(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE Conversions;
(*
  Module:	numeric-to-string conversions
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*)

  PROCEDURE ConvertNum(num, len, base: CARDINAL;
		       neg: BOOLEAN;
		       VAR str: ARRAY OF CHAR);
    VAR i: CARDINAL;
	r: CARDINAL;
	tmp: ARRAY [0..20] OF CHAR;
    BEGIN
	i := 0;
	REPEAT
		r := num MOD base;
		num := num DIV base;
		IF r <= 9 THEN
			tmp[i] := CHR(r + ORD('0'));
		ELSE
			tmp[i] := CHR(r - 10 + ORD('A'));
		END;
		INC(i);
	UNTIL num = 0;
	IF neg THEN
		tmp[i] := '-';
		INC(i)
	END;
	IF len > HIGH(str) + 1 THEN len := HIGH(str) + 1; END;
	IF i > HIGH(str) + 1 THEN i := HIGH(str) + 1; END;
	r := 0;
	WHILE len > i DO str[r] := ' '; INC(r); DEC(len); END;
	WHILE i > 0 DO str[r] := tmp[i-1]; DEC(i); INC(r); END;
	WHILE r <= HIGH(str) DO
		str[r] := 0C;
		INC(r);
	END;
    END ConvertNum;

  PROCEDURE ConvertOctal(num, len: CARDINAL; VAR str: ARRAY OF CHAR);
  BEGIN   
	ConvertNum(num, len, 8, FALSE, str);
  END ConvertOctal;   

  PROCEDURE ConvertHex(num, len: CARDINAL; VAR str: ARRAY OF CHAR);
  BEGIN   
	ConvertNum(num, len, 16, FALSE, str);
  END ConvertHex;   

  PROCEDURE ConvertCardinal(num, len: CARDINAL; VAR str: ARRAY OF CHAR);   
  BEGIN   
	ConvertNum(num, len, 10, FALSE, str);
  END ConvertCardinal;   

  PROCEDURE ConvertInteger(num: INTEGER;
			   len: CARDINAL;   
                           VAR str: ARRAY OF CHAR); 
  BEGIN 
	IF (num < 0) AND (num >= -MAX(INTEGER)) THEN
		ConvertNum(-num, len, 10, TRUE, str);
	ELSE
		ConvertNum(CARDINAL(num), len, 10, num < 0, str);
	END;
  END ConvertInteger; 

END Conversions.

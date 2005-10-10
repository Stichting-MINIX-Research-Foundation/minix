(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE MathLib0;
(*
  Module:	Some mathematical functions
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*)

  IMPORT	Mathlib;

  PROCEDURE cos(arg: REAL): REAL;
  BEGIN
	RETURN Mathlib.cos(arg);
  END cos;

  PROCEDURE sin(arg: REAL): REAL;
  BEGIN
	RETURN Mathlib.sin(arg);
  END sin;

  PROCEDURE arctan(arg: REAL): REAL;
  BEGIN
	RETURN Mathlib.arctan(arg);
  END arctan;

  PROCEDURE sqrt(arg: REAL): REAL;
  BEGIN
	RETURN Mathlib.sqrt(arg);
  END sqrt;

  PROCEDURE ln(arg: REAL): REAL;
  BEGIN
	RETURN Mathlib.ln(arg);
  END ln;

  PROCEDURE exp(arg: REAL): REAL;
  BEGIN
	RETURN Mathlib.exp(arg);
  END exp;

  PROCEDURE entier(x: REAL): INTEGER;
  VAR i: INTEGER;
  BEGIN
	IF x < 0.0 THEN
		i := TRUNC(-x);
		IF FLOAT(i) = -x THEN
			RETURN -i;
		ELSE
			RETURN -i -1;
		END;
	END;
	RETURN TRUNC(x);
  END entier;

  PROCEDURE real(x: INTEGER): REAL;
  BEGIN
	IF x < 0 THEN
		RETURN - FLOAT(-x);
	END;
	RETURN FLOAT(x);
  END real;

BEGIN
END MathLib0.

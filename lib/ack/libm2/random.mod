(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE random;
(*
  Module:       random numbers
  Author:       Ceriel J.H. Jacobs
  Version:      $Header$
*)

  FROM	Unix IMPORT	getpid, time;
  TYPE index = [1..55];

  VAR	X: ARRAY index OF CARDINAL;
	j, k: index;
	tm: LONGINT;

  PROCEDURE Random(): CARDINAL;
  BEGIN
	IF k-1 <= 0 THEN k := 55; ELSE DEC(k) END;
	IF j-1 <= 0 THEN j := 55; ELSE DEC(j) END;
	X[k] := X[k] + X[j];
	RETURN X[k]
  END Random;

  PROCEDURE Uniform (lwb, upb: CARDINAL): CARDINAL;
  BEGIN
    	IF upb <= lwb THEN RETURN lwb; END;
    	RETURN lwb + (Random() MOD (upb - lwb + 1));
  END Uniform;

  PROCEDURE StartSeed(seed: CARDINAL);
  VAR v: CARDINAL;
  BEGIN
	FOR k := 1 TO 55 DO
		seed := 1297 * seed + 123;
		X[k] := seed;
	END;
	FOR k := 1 TO 15 DO
		j := tm MOD 55D + 1D;
		v := X[j];
		tm := tm DIV 7D;
		j := tm MOD 55D + 1D;
		X[j] := v;
		tm := tm * 3D;
	END;
	k := 1;
	j := 25;
  END StartSeed;

BEGIN
 	tm := time(NIL);
	X[1] := tm;
	StartSeed(CARDINAL(getpid()) * X[1]);
END random.

(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE Mathlib;
(*
  Module:	Mathematical functions
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*)

  FROM	EM IMPORT	FIF, FEF;
  FROM	Traps IMPORT	Message;

  CONST
	OneRadianInDegrees	= 57.295779513082320876798155D;
	OneDegreeInRadians	=  0.017453292519943295769237D;
	OneOverSqrt2		= 0.70710678118654752440084436210484904D;

  (* basic functions *)

  PROCEDURE pow(x: REAL; i: INTEGER): REAL;
  BEGIN
	RETURN SHORT(longpow(LONG(x), i));
  END pow;

  PROCEDURE longpow(x: LONGREAL; i: INTEGER): LONGREAL;
    VAR	val: LONGREAL;
	ri: LONGREAL;
  BEGIN
	ri := FLOATD(i);
	IF x < 0.0D THEN
		val := longexp(longln(-x) * ri);
		IF ODD(i) THEN RETURN -val;
		ELSE RETURN val;
		END;
	ELSIF x = 0.0D THEN
		RETURN 0.0D;
	ELSE
		RETURN longexp(longln(x) * ri);
	END;
  END longpow;

  PROCEDURE sqrt(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longsqrt(LONG(x)));
  END sqrt;

  PROCEDURE longsqrt(x: LONGREAL): LONGREAL;
    VAR
	temp: LONGREAL;
	exp, i: INTEGER;
  BEGIN
	IF x <= 0.0D THEN
		IF x < 0.0D THEN
			Message("sqrt: negative argument");
			HALT
		END;
		RETURN 0.0D;
	END;
	temp := FEF(x,exp);
	(*
	 * NOTE
	 * this wont work on 1's comp
	 *)
	IF ODD(exp) THEN
		temp := 2.0D * temp;
		DEC(exp);
	END;
	temp := 0.5D*(1.0D + temp);

	WHILE exp > 28 DO
		temp := temp * 16384.0D;
		exp := exp - 28;
	END;
	WHILE exp < -28 DO
		temp := temp / 16384.0D;
		exp := exp + 28;
	END;
	WHILE exp >= 2 DO
		temp := temp * 2.0D;
		exp := exp - 2;
	END;
	WHILE exp <= -2 DO
		temp := temp / 2.0D;
		exp := exp + 2;
	END;
	FOR i := 0 TO 5 DO
		temp := 0.5D*(temp + x/temp);
	END;
	RETURN temp;
  END longsqrt;

  PROCEDURE ldexp(x:LONGREAL; n: INTEGER): LONGREAL;
  BEGIN
	WHILE n >= 16 DO
		x := x * 65536.0D;
		n := n - 16;
	END;
	WHILE n > 0 DO
		x := x * 2.0D;
		DEC(n);
	END;
	WHILE n <= -16 DO
		x := x / 65536.0D;
		n := n + 16;
	END;
	WHILE n < 0 DO
		x := x / 2.0D;
		INC(n);
	END;
	RETURN x;
  END ldexp;

  PROCEDURE exp(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longexp(LONG(x)));
  END exp;

  PROCEDURE longexp(x: LONGREAL): LONGREAL;
  (*	Algorithm and coefficients from:
		"Software manual for the elementary functions"
		by W.J. Cody and W. Waite, Prentice-Hall, 1980
  *)
    CONST
	p0 = 0.25000000000000000000D+00;
	p1 = 0.75753180159422776666D-02;
	p2 = 0.31555192765684646356D-04;
	q0 = 0.50000000000000000000D+00;
	q1 = 0.56817302698551221787D-01;
	q2 = 0.63121894374398503557D-03;
	q3 = 0.75104028399870046114D-06;

    VAR
	neg: BOOLEAN;
	n: INTEGER;
	xn, g, x1, x2: LONGREAL;
  BEGIN
	neg := x < 0.0D;
	IF neg THEN
		x := -x;
	END;
	n := TRUNC(x/longln2 + 0.5D);
	xn := FLOATD(n);
	x1 := FLOATD(TRUNCD(x));
	x2 := x - x1;
	g := ((x1 - xn * 0.693359375D)+x2) - xn * (-2.1219444005469058277D-4);
	IF neg THEN
		g := -g;
		n := -n;
	END;
	xn := g*g;
	x := g*((p2*xn+p1)*xn+p0);
	INC(n);
	RETURN ldexp(0.5D + x/((((q3*xn+q2)*xn+q1)*xn+q0) - x), n);
  END longexp;

  PROCEDURE ln(x: REAL): REAL;	(* natural log *)
  BEGIN
	RETURN SHORT(longln(LONG(x)));
  END ln;

  PROCEDURE longln(x: LONGREAL): LONGREAL;	(* natural log *)
  (*	Algorithm and coefficients from:
		"Software manual for the elementary functions"
		by W.J. Cody and W. Waite, Prentice-Hall, 1980
   *)
    CONST
	p0 = -0.64124943423745581147D+02;
	p1 =  0.16383943563021534222D+02;
	p2 = -0.78956112887491257267D+00;
	q0 = -0.76949932108494879777D+03;
	q1 =  0.31203222091924532844D+03;
	q2 = -0.35667977739034646171D+02;
	q3 =  1.0D;
    VAR
	exp: INTEGER;
	z, znum, zden, w: LONGREAL;

  BEGIN
	IF x <= 0.0D THEN
		Message("ln: argument <= 0");
		HALT
	END;
	x := FEF(x, exp);
	IF x > OneOverSqrt2 THEN
		znum := (x - 0.5D) - 0.5D;
		zden := x * 0.5D + 0.5D;
	ELSE
		znum := x - 0.5D;
		zden := znum * 0.5D + 0.5D;
		DEC(exp);
	END;
	z := znum / zden;
	w := z * z;
	x := z + z * w * (((p2*w+p1)*w+p0)/(((q3*w+q2)*w+q1)*w+q0));
	z := FLOATD(exp);
	x := x + z * (-2.121944400546905827679D-4);
	RETURN x + z * 0.693359375D;
  END longln;

  PROCEDURE log(x: REAL): REAL;	(* log with base 10 *)
  BEGIN
	RETURN SHORT(longlog(LONG(x)));
  END log;

  PROCEDURE longlog(x: LONGREAL): LONGREAL;	(* log with base 10 *)
  BEGIN
	RETURN longln(x)/longln10;
  END longlog;

  (* trigonometric functions; arguments in radians *)

  PROCEDURE sin(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longsin(LONG(x)));
  END sin;

  PROCEDURE sinus(x: LONGREAL; cosflag: BOOLEAN) : LONGREAL;
  (*	Algorithm and coefficients from:
		"Software manual for the elementary functions"
		by W.J. Cody and W. Waite, Prentice-Hall, 1980
  *)
    CONST
	r0 = -0.16666666666666665052D+00;
	r1 =  0.83333333333331650314D-02;
	r2 = -0.19841269841201840457D-03;
	r3 =  0.27557319210152756119D-05;
	r4 = -0.25052106798274584544D-07;
	r5 =  0.16058936490371589114D-09;
	r6 = -0.76429178068910467734D-12;
	r7 =  0.27204790957888846175D-14;
	A1 =  3.1416015625D;
	A2 = -8.908910206761537356617D-6;
    VAR
	x1, x2, y : LONGREAL;
	neg : BOOLEAN;
  BEGIN
	IF x < 0.0D THEN
		neg := TRUE;
		x := -x
	ELSE	neg := FALSE
	END;
	IF cosflag THEN
		neg := FALSE;
		y := longhalfpi + x
	ELSE
		y := x
	END;
	y := y / longpi + 0.5D;

	IF FIF(y, 1.0D, y) < 0.0D THEN ; END;
	IF FIF(y, 0.5D, x1) # 0.0D THEN neg := NOT neg END;
	IF cosflag THEN y := y - 0.5D END;
	x2 := FIF(x, 1.0, x1);
	x := x1 - y * A1;
	x := x + x2;
	x := x - y * A2;

	IF x < 0.0D THEN
		neg := NOT neg;
		x := -x
	END;
	y := x * x;
	x := x + x * y * (((((((r7*y+r6)*y+r5)*y+r4)*y+r3)*y+r2)*y+r1)*y+r0);
	IF neg THEN RETURN -x END;
	RETURN x;
  END sinus;

  PROCEDURE longsin(x: LONGREAL): LONGREAL;
  BEGIN
	RETURN sinus(x, FALSE);
  END longsin;

  PROCEDURE cos(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longcos(LONG(x)));
  END cos;

  PROCEDURE longcos(x: LONGREAL): LONGREAL;
  BEGIN
	IF x < 0.0D THEN x := -x; END;
	RETURN sinus(x, TRUE);	
  END longcos;

  PROCEDURE tan(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longtan(LONG(x)));
  END tan;

  PROCEDURE longtan(x: LONGREAL): LONGREAL;
  (*	Algorithm and coefficients from:
		"Software manual for the elementary functions"
		by W.J. Cody and W. Waite, Prentice-Hall, 1980
  *)

    CONST
	p1 = -0.13338350006421960681D+00;
	p2 =  0.34248878235890589960D-02;
	p3 = -0.17861707342254426711D-04;

	q0 =  1.0D;
	q1 = -0.46671683339755294240D+00;
	q2 =  0.25663832289440112864D-01;
	q3 = -0.31181531907010027307D-03;
	q4 =  0.49819433993786512270D-06;

	A1 =  1.57080078125D;
	A2 = -4.454455103380768678308D-06;

    VAR y, x1, x2: LONGREAL;
	negative: BOOLEAN;
	invert: BOOLEAN;
  BEGIN
	negative := x < 0.0D;
	y := x / longhalfpi + 0.5D;

        (*      Use extended precision to calculate reduced argument.
                Here we used 12 bits of the mantissa for a1.
                Also split x in integer part x1 and fraction part x2.
        *)
	IF FIF(y, 1.0D, y) < 0.0D THEN ; END;
	invert := FIF(y, 0.5D, x1) # 0.0D;
	x2 := FIF(x, 1.0D, x1);
	x := x1 - y * A1;
	x := x + x2;
	x := x - y * A2;

	y := x * x;
	x := x + x * y * ((p3*y+p2)*y+p1);
	y := (((q4*y+q3)*y+q2)*y+q1)*y+q0;
	IF negative THEN x := -x END;
	IF invert THEN RETURN -y/x END;
	RETURN x/y;
  END longtan;

  PROCEDURE arcsin(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longarcsin(LONG(x)));
  END arcsin;

  PROCEDURE arcsincos(x: LONGREAL; cosfl: BOOLEAN): LONGREAL;
    CONST
	p0 = -0.27368494524164255994D+02;
	p1 =  0.57208227877891731407D+02;
	p2 = -0.39688862997540877339D+02;
	p3 =  0.10152522233806463645D+02;
	p4 = -0.69674573447350646411D+00;

	q0 = -0.16421096714498560795D+03;
	q1 =  0.41714430248260412556D+03;
	q2 = -0.38186303361750149284D+03;
	q3 =  0.15095270841030604719D+03;
	q4 = -0.23823859153670238830D+02;
	q5 =  1.0D;
    VAR
	negative : BOOLEAN;
	big: BOOLEAN;
	g: LONGREAL;
  BEGIN
	negative := x < 0.0D;
	IF negative THEN x := -x; END;
	IF x > 0.5D THEN
		big := TRUE;
		IF x > 1.0D THEN
			Message("arcsin or arccos: argument > 1");
			HALT
		END;
		g := 0.5D - 0.5D * x;
		x := -longsqrt(g);
		x := x + x;
	ELSE
		big := FALSE;
		g := x * x;
	END;
	x := x + x * g *
	  ((((p4*g+p3)*g+p2)*g+p1)*g+p0)/(((((q5*g+q4)*g+q3)*g+q2)*g+q1)*g+q0);
	IF cosfl AND NOT negative THEN x := -x END;
	IF cosfl = NOT big THEN
		x := (x + longquartpi) + longquartpi;
	ELSIF cosfl AND negative AND big THEN
		x := (x + longhalfpi) + longhalfpi;
	END;
	IF negative AND NOT cosfl THEN x := -x END;
	RETURN x;
  END arcsincos;	

  PROCEDURE longarcsin(x: LONGREAL): LONGREAL;
  BEGIN
	RETURN arcsincos(x, FALSE);
  END longarcsin;

  PROCEDURE arccos(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longarccos(LONG(x)));
  END arccos;

  PROCEDURE longarccos(x: LONGREAL): LONGREAL;
  BEGIN
	RETURN arcsincos(x, TRUE);
  END longarccos;

  PROCEDURE arctan(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longarctan(LONG(x)));
  END arctan;

  VAR A: ARRAY[0..3] OF LONGREAL;
      arctaninit: BOOLEAN;

  PROCEDURE longarctan(x: LONGREAL): LONGREAL;
  (*	Algorithm and coefficients from:
		"Software manual for the elementary functions"
		by W.J. Cody and W. Waite, Prentice-Hall, 1980
  *)
    CONST
	p0 = -0.13688768894191926929D+02;
	p1 = -0.20505855195861651981D+02;
	p2 = -0.84946240351320683534D+01;
	p3 = -0.83758299368150059274D+00;
	q0 =  0.41066306682575781263D+02;
	q1 =  0.86157349597130242515D+02;
	q2 =  0.59578436142597344465D+02;
	q3 =  0.15024001160028576121D+02;
	q4 =  1.0D;
    VAR
	g: LONGREAL;
	neg: BOOLEAN;
	n: INTEGER;
  BEGIN
	IF NOT arctaninit THEN
		arctaninit := TRUE;
		A[0] := 0.0D;
		A[1] := 0.52359877559829887307710723554658381D;	(* p1/6 *)
		A[2] := longhalfpi;
		A[3] := 1.04719755119659774615421446109316763D; (* pi/3 *)
	END;
	neg := FALSE;
	IF x < 0.0D THEN
		neg := TRUE;
		x := -x;
	END;
	IF x > 1.0D THEN
		x := 1.0D/x;
		n := 2
	ELSE
		n := 0
	END;
	IF x > 0.26794919243112270647D (* 2-sqrt(3) *) THEN
		INC(n);
		x := (((0.73205080756887729353D*x-0.5D)-0.5D)+x)/
			(1.73205080756887729353D + x);
	END;
	g := x*x;
	x := x + x * g * (((p3*g+p2)*g+p1)*g+p0) / ((((q4*g+q3)*g+q2)*g+q1)*g+q0);
	IF n > 1 THEN x := -x END;
	x := x + A[n];
	IF neg THEN RETURN -x; END;
	RETURN x;
  END longarctan;

  (* hyperbolic functions *)
  (* The C math library has better implementations for some of these, but
     they depend on some properties of the floating point implementation,
     and, for now, we don't want that in the Modula-2 system.
  *)

  PROCEDURE sinh(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longsinh(LONG(x)));
  END sinh;

  PROCEDURE longsinh(x: LONGREAL): LONGREAL;
    VAR expx: LONGREAL;
  BEGIN
	expx := longexp(x);
	RETURN (expx - 1.0D/expx)/2.0D;
  END longsinh;

  PROCEDURE cosh(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longcosh(LONG(x)));
  END cosh;

  PROCEDURE longcosh(x: LONGREAL): LONGREAL;
    VAR expx: LONGREAL;
  BEGIN
	expx := longexp(x);
	RETURN (expx + 1.0D/expx)/2.0D;
  END longcosh;

  PROCEDURE tanh(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longtanh(LONG(x)));
  END tanh;

  PROCEDURE longtanh(x: LONGREAL): LONGREAL;
    VAR expx: LONGREAL;
  BEGIN
	expx := longexp(x);
	RETURN (expx - 1.0D/expx) / (expx + 1.0D/expx);
  END longtanh;

  PROCEDURE arcsinh(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longarcsinh(LONG(x)));
  END arcsinh;

  PROCEDURE longarcsinh(x: LONGREAL): LONGREAL;
    VAR neg: BOOLEAN;
  BEGIN
	neg := FALSE;
	IF x < 0.0D THEN
		neg := TRUE;
		x := -x;
	END;
	x := longln(x + longsqrt(x*x+1.0D));
	IF neg THEN RETURN -x; END;
	RETURN x;
  END longarcsinh;

  PROCEDURE arccosh(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longarccosh(LONG(x)));
  END arccosh;

  PROCEDURE longarccosh(x: LONGREAL): LONGREAL;
  BEGIN
	IF x < 1.0D THEN
		Message("arccosh: argument < 1");
		HALT
	END;
	RETURN longln(x + longsqrt(x*x - 1.0D));
  END longarccosh;

  PROCEDURE arctanh(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longarctanh(LONG(x)));
  END arctanh;

  PROCEDURE longarctanh(x: LONGREAL): LONGREAL;
  BEGIN
	IF (x <= -1.0D) OR (x >= 1.0D) THEN
		Message("arctanh: ABS(argument) >= 1");
		HALT
	END;
	RETURN longln((1.0D + x)/(1.0D - x)) / 2.0D;
  END longarctanh;

  (* conversions *)

  PROCEDURE RadianToDegree(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longRadianToDegree(LONG(x)));
  END RadianToDegree;

  PROCEDURE longRadianToDegree(x: LONGREAL): LONGREAL;
  BEGIN
	RETURN x * OneRadianInDegrees;
  END longRadianToDegree;

  PROCEDURE DegreeToRadian(x: REAL): REAL;
  BEGIN
	RETURN SHORT(longDegreeToRadian(LONG(x)));
  END DegreeToRadian;

  PROCEDURE longDegreeToRadian(x: LONGREAL): LONGREAL;
  BEGIN
	RETURN x * OneDegreeInRadians;
  END longDegreeToRadian;

BEGIN
	arctaninit := FALSE;
END Mathlib.

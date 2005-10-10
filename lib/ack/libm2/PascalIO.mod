(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE PascalIO;
(*
  Module:	Pascal-like Input/Output
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*)

  FROM	Conversions IMPORT
			ConvertInteger, ConvertCardinal;
  FROM	RealConversions IMPORT
			LongRealToString, StringToLongReal;
  FROM	Traps IMPORT	Message;
  FROM	Streams IMPORT	Stream, StreamKind, StreamMode, StreamResult,
			InputStream, OutputStream, OpenStream, CloseStream, 
			EndOfStream, Read, Write, StreamBuffering;
  FROM	Storage IMPORT	Allocate;
  FROM	SYSTEM IMPORT	ADR;

  TYPE	charset = SET OF CHAR;
	btype = (Preading, Pwriting, free);

  CONST	spaces = charset{11C, 12C, 13C, 14C, 15C, ' '};

  TYPE	IOstream = RECORD
			type: btype;
			done, eof : BOOLEAN;
			ch: CHAR;
			next: Text;
			stream: Stream;
		END;
	Text =	POINTER TO IOstream;
	numbuf = ARRAY[0..255] OF CHAR;

  VAR	ibuf, obuf: IOstream;
	head: Text;
	result: StreamResult;

  PROCEDURE Reset(VAR InputText: Text; Filename: ARRAY OF CHAR);
  BEGIN
	doclose(InputText);
	getstruct(InputText);
	WITH InputText^ DO
		OpenStream(stream, Filename, text, reading, result);
		IF result # succeeded THEN
			Message("could not open input file");
			HALT;
		END;
		type := Preading;
		done := FALSE;
		eof := FALSE;
	END;
  END Reset;

  PROCEDURE Rewrite(VAR OutputText: Text; Filename: ARRAY OF CHAR);
  BEGIN
	doclose(OutputText);
	getstruct(OutputText);
	WITH OutputText^ DO
		OpenStream(stream, Filename, text, writing, result);
		IF result # succeeded THEN
			Message("could not open output file");
			HALT;
		END;
		type := Pwriting;
	END;
  END Rewrite;

  PROCEDURE CloseOutput();
  VAR p: Text;
  BEGIN
	p := head;
	WHILE p # NIL DO
		doclose(p);
		p := p^.next;
	END;
  END CloseOutput;

  PROCEDURE doclose(Xtext: Text);
  BEGIN
	IF Xtext # Notext THEN
		WITH Xtext^ DO
			IF type # free THEN
				CloseStream(stream, result);
				type := free;
			END;
		END;
	END;
  END doclose;

  PROCEDURE getstruct(VAR Xtext: Text);
  BEGIN
	Xtext := head;
	WHILE (Xtext # NIL) AND (Xtext^.type # free) DO
		Xtext := Xtext^.next;
	END;
	IF Xtext = NIL THEN
		Allocate(Xtext,SIZE(IOstream));
		Xtext^.next := head;
		head := Xtext;
	END;
  END getstruct;

  PROCEDURE Error(tp: btype);
  BEGIN
	IF tp = Preading THEN
		Message("input text expected");
	ELSE
		Message("output text expected");
	END;
	HALT;
  END Error;

  PROCEDURE ReadChar(InputText: Text; VAR ch : CHAR);
  BEGIN
	ch := NextChar(InputText);
	IF InputText^.eof THEN
		Message("unexpected EOF");
		HALT;
	END;
	InputText^.done := FALSE;
  END ReadChar;

  PROCEDURE NextChar(InputText: Text): CHAR;
  BEGIN
	WITH InputText^ DO
		IF type # Preading THEN Error(Preading); END;
		IF NOT done THEN
			IF EndOfStream(stream, result) THEN
				eof := TRUE;
				ch := 0C;
			ELSE
				Read(stream, ch, result);
				done := TRUE;
			END;
		END;
		RETURN ch;
	END;
  END NextChar;

  PROCEDURE Get(InputText: Text);
  VAR dummy: CHAR;
  BEGIN
	ReadChar(InputText, dummy);
  END Get;

  PROCEDURE Eoln(InputText: Text): BOOLEAN;
  BEGIN
	RETURN NextChar(InputText) = 12C;
  END Eoln;

  PROCEDURE Eof(InputText: Text): BOOLEAN;
  BEGIN
	RETURN (NextChar(InputText) = 0C) AND InputText^.eof;
  END Eof;

  PROCEDURE ReadLn(InputText: Text);
  VAR ch: CHAR;
  BEGIN
	REPEAT
		ReadChar(InputText, ch)
	UNTIL ch = 12C;
  END ReadLn;

  PROCEDURE WriteChar(OutputText: Text; char: CHAR);
  BEGIN
	WITH OutputText^ DO
		IF type # Pwriting THEN Error(Pwriting); END;
		Write(stream, char, result);
	END;
  END WriteChar;

  PROCEDURE WriteLn(OutputText: Text);
  BEGIN
	WriteChar(OutputText, 12C);
  END WriteLn;

  PROCEDURE Page(OutputText: Text);
  BEGIN
	WriteChar(OutputText, 14C);
  END Page;

  PROCEDURE ReadInteger(InputText: Text; VAR int : INTEGER);
  CONST
    	SAFELIMITDIV10 = MAX(INTEGER) DIV 10;
    	SAFELIMITREM10 = MAX(INTEGER) MOD 10;
  VAR
    	neg : BOOLEAN;
    	safedigit: CARDINAL;
	ch: CHAR;
    	chvalue: CARDINAL;
  BEGIN
    	WHILE NextChar(InputText) IN spaces DO
		Get(InputText);
	END;
	ch := NextChar(InputText);
    	IF ch = '-' THEN
		Get(InputText);
		ch := NextChar(InputText);
		neg := TRUE;
    	ELSIF ch = '+' THEN
		Get(InputText);
		ch := NextChar(InputText);
		neg := FALSE;
    	ELSE
		neg := FALSE
    	END;

    	safedigit := SAFELIMITREM10;
    	IF neg THEN safedigit := safedigit + 1 END;
    	int := 0;
	IF (ch >= '0') AND (ch <= '9') THEN
		WHILE (ch >= '0') & (ch <= '9') DO
  			chvalue := ORD(ch) - ORD('0');
	   		IF (int < -SAFELIMITDIV10) OR 
		   	   ( (int = -SAFELIMITDIV10) AND
		     	     (chvalue > safedigit)) THEN
				Message("integer too large");
				HALT;
	    		ELSE
				int := 10*int - VAL(INTEGER, chvalue);
				Get(InputText);
				ch := NextChar(InputText);
	    		END;
		END;
		IF NOT neg THEN
   			int := -int
		END;
	ELSE
		Message("integer expected");
		HALT;
	END;
  END ReadInteger;

  PROCEDURE ReadCardinal(InputText: Text; VAR card : CARDINAL);
  CONST
    	SAFELIMITDIV10 = MAX(CARDINAL) DIV 10;
    	SAFELIMITREM10 = MAX(CARDINAL) MOD 10;

  VAR
    	ch : CHAR;
    	safedigit: CARDINAL;
    	chvalue: CARDINAL;
  BEGIN
    	WHILE NextChar(InputText) IN spaces DO
		Get(InputText);
	END;
	ch := NextChar(InputText);
    	safedigit := SAFELIMITREM10;
    	card := 0;
	IF (ch >= '0') AND (ch <= '9') THEN
		WHILE (ch >= '0') & (ch <= '9') DO
  			chvalue := ORD(ch) - ORD('0');
	    		IF (card > SAFELIMITDIV10) OR 
			   ( (card = SAFELIMITDIV10) AND
			     (chvalue > safedigit)) THEN
				Message("cardinal too large");
				HALT;
		    	ELSE
				card := 10*card + chvalue;
				Get(InputText);
				ch := NextChar(InputText);
		    	END;
		END;
	ELSE
		Message("cardinal expected");
		HALT;
	END;
  END ReadCardinal;

  PROCEDURE ReadReal(InputText: Text; VAR real: REAL);
  VAR x1: LONGREAL;
  BEGIN
	ReadLongReal(InputText, x1);
	real := x1
  END ReadReal;

  PROCEDURE ReadLongReal(InputText: Text; VAR real: LONGREAL);
  VAR
	buf: numbuf;
	ch: CHAR;
	ok: BOOLEAN;
	index: INTEGER;

    PROCEDURE inch(): CHAR;
    BEGIN
	buf[index] := ch;
	INC(index);
	Get(InputText);
	RETURN NextChar(InputText);
    END inch;

  BEGIN
	index := 0;
	ok := TRUE;
    	WHILE NextChar(InputText) IN spaces DO
		Get(InputText);
	END;
	ch := NextChar(InputText);
	IF (ch ='+') OR (ch = '-') THEN
		ch := inch();
	END;
	IF (ch >= '0') AND (ch <= '9') THEN
		WHILE (ch >= '0') AND (ch <= '9') DO
			ch := inch();
		END;
		IF (ch = '.') THEN
			ch := inch();
			IF (ch >= '0') AND (ch <= '9') THEN
				WHILE (ch >= '0') AND (ch <= '9') DO
					ch := inch();
				END;
			ELSE
				ok := FALSE;
			END;
		END;
		IF ok AND (ch = 'E') THEN
			ch := inch();
			IF (ch ='+') OR (ch = '-') THEN
				ch := inch();
			END;
			IF (ch >= '0') AND (ch <= '9') THEN
				WHILE (ch >= '0') AND (ch <= '9') DO
					ch := inch();
				END;
			ELSE
				ok := FALSE;
			END;
		END;
	ELSE
		ok := FALSE;
	END;
	IF ok THEN
		buf[index] := 0C;
		StringToLongReal(buf, real, ok);
	END;
	IF NOT ok THEN
		Message("Illegal real");
		HALT;
	END;
  END ReadLongReal;

  PROCEDURE WriteCardinal(OutputText: Text; card: CARDINAL; width: CARDINAL);
  VAR
    	buf : numbuf;
  BEGIN
	ConvertCardinal(card, 1, buf);
	WriteString(OutputText, buf, width);
  END WriteCardinal;

  PROCEDURE WriteInteger(OutputText: Text; int: INTEGER; width: CARDINAL);
  VAR
    	buf : numbuf;
  BEGIN
    	ConvertInteger(int, 1, buf);
	WriteString(OutputText, buf, width);
  END WriteInteger;

  PROCEDURE WriteBoolean(OutputText: Text; bool: BOOLEAN; width: CARDINAL);
  BEGIN
	IF bool THEN
		WriteString(OutputText, " TRUE", width);
	ELSE
		WriteString(OutputText, "FALSE", width);
	END;
  END WriteBoolean;

  PROCEDURE WriteReal(OutputText: Text; real: REAL; width, nfrac: CARDINAL);
  BEGIN
	WriteLongReal(OutputText, LONG(real), width, nfrac)
  END WriteReal;

  PROCEDURE WriteLongReal(OutputText: Text; real: LONGREAL; width, nfrac: CARDINAL);
  VAR
	buf: numbuf;
	ok: BOOLEAN;
	digits: INTEGER;
  BEGIN
	IF width > SIZE(buf) THEN
		width := SIZE(buf);
	END;
	IF nfrac > 0 THEN
		LongRealToString(real, width, nfrac, buf, ok);
	ELSE
		IF width < 9 THEN width := 9; END;
		IF real < 0.0D THEN
			digits := 7 - INTEGER(width);
		ELSE
			digits := 6 - INTEGER(width);
		END;
		LongRealToString(real, width, digits, buf, ok);
	END;
	WriteString(OutputText, buf, 0);
  END WriteLongReal;

  PROCEDURE WriteString(OutputText: Text; str: ARRAY OF CHAR; width: CARDINAL);
  VAR index: CARDINAL;
  BEGIN
	index := 0;
	WHILE (index <= HIGH(str)) AND (str[index] # Eos) DO
		INC(index);
	END;
	WHILE index < width DO
		WriteChar(OutputText, " ");
		INC(index);
	END;
	index := 0;
	WHILE (index <= HIGH(str)) AND (str[index] # Eos) DO
		WriteChar(OutputText, str[index]);
		INC(index);
	END;
  END WriteString;

BEGIN	(* PascalIO initialization *)
	WITH ibuf DO
		stream := InputStream;
		eof := FALSE;
		type := Preading;
		done := FALSE;
	END;
	WITH obuf DO
		stream := OutputStream;
		eof := FALSE;
		type := Pwriting;
	END;
	Notext := NIL;
	Input := ADR(ibuf);
	Output := ADR(obuf);
	Input^.next := Output;
	Output^.next := NIL;
	head := Input;
END PascalIO.

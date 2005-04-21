(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE InOut ;
(*
  Module:	Wirth's Input/Output module
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*)

  IMPORT	Streams;
  FROM	Conversions IMPORT
			ConvertCardinal, ConvertInteger,
			ConvertOctal, ConvertHex;
  FROM	Traps IMPORT	Message;

  CONST	TAB = 11C;

  TYPE	numbuf = ARRAY[0..255] OF CHAR;

  VAR	unread: BOOLEAN;
	unreadch: CHAR;
	CurrIn, CurrOut: Streams.Stream;
	result: Streams.StreamResult;

  PROCEDURE Read(VAR c : CHAR);

  BEGIN
	IF unread THEN
		unread := FALSE;
		c := unreadch;
		Done := TRUE;
	ELSE
		Streams.Read(CurrIn, c, result);
		Done := result = Streams.succeeded;
	END;
  END Read;

  PROCEDURE UnRead(ch: CHAR);
  BEGIN
	unread := TRUE;
	unreadch := ch;
  END UnRead;

  PROCEDURE Write(c: CHAR);
  BEGIN
	Streams.Write(CurrOut, c, result);
  END Write;

  PROCEDURE OpenInput(defext: ARRAY OF CHAR);
  VAR namebuf : ARRAY [1..128] OF CHAR;
  BEGIN
	IF CurrIn # Streams.InputStream THEN
		Streams.CloseStream(CurrIn, result);
	END;
	MakeFileName("Name of input file: ", defext, namebuf);
	IF NOT Done THEN RETURN; END;
	openinput(namebuf);
  END OpenInput;

  PROCEDURE OpenInputFile(filename: ARRAY OF CHAR);
  BEGIN
	IF CurrIn # Streams.InputStream THEN
		Streams.CloseStream(CurrIn, result);
	END;
	openinput(filename);
  END OpenInputFile;

  PROCEDURE openinput(namebuf: ARRAY OF CHAR);
  BEGIN
	IF (namebuf[0] = '-') AND (namebuf[1] = 0C) THEN
		CurrIn := Streams.InputStream;
		Done := TRUE;
	ELSE
		Streams.OpenStream(CurrIn, namebuf, Streams.text,
				   Streams.reading, result);
		Done := result = Streams.succeeded;
	END;
  END openinput;

  PROCEDURE CloseInput;
  BEGIN
	IF CurrIn # Streams.InputStream THEN
		Streams.CloseStream(CurrIn, result);
	END;
	CurrIn := Streams.InputStream;
  END CloseInput;

  PROCEDURE OpenOutput(defext: ARRAY OF CHAR);
  VAR namebuf : ARRAY [1..128] OF CHAR;
  BEGIN
	IF CurrOut # Streams.OutputStream THEN
		Streams.CloseStream(CurrOut, result);
	END;
	MakeFileName("Name of output file: ", defext, namebuf);
	IF NOT Done THEN RETURN; END;
	openoutput(namebuf);
  END OpenOutput;

  PROCEDURE OpenOutputFile(filename: ARRAY OF CHAR);
  BEGIN
	IF CurrOut # Streams.OutputStream THEN
		Streams.CloseStream(CurrOut, result);
	END;
	openoutput(filename);
  END OpenOutputFile;

  PROCEDURE openoutput(namebuf: ARRAY OF CHAR);
  BEGIN
	IF (namebuf[1] = '-') AND (namebuf[2] = 0C) THEN
		CurrOut := Streams.OutputStream;
		Done := TRUE;
	ELSE
		Streams.OpenStream(CurrOut, namebuf, Streams.text,
				   Streams.writing, result);
		Done := result = Streams.succeeded;
	END;
  END openoutput;

  PROCEDURE CloseOutput;
  BEGIN
	IF CurrOut # Streams.OutputStream THEN
		Streams.CloseStream(CurrOut, result);
	END;
	CurrOut := Streams.OutputStream;
  END CloseOutput;

  PROCEDURE MakeFileName(prompt, defext : ARRAY OF CHAR;
		       VAR buf : ARRAY OF CHAR);
  VAR	i : INTEGER;
	j : CARDINAL;
  BEGIN
	Done := TRUE;
	IF Streams.isatty(Streams.InputStream, result) THEN
		XWriteString(prompt);
	END;
	XReadString(buf);
	i := 0;
	WHILE buf[i] # 0C DO i := i + 1 END;
	IF i # 0 THEN
		i := i - 1;
		IF buf[i] = '.' THEN
	    		FOR j := 0 TO HIGH(defext) DO
				i := i + 1;
				buf[i] := defext[j];
	    		END;
	    		buf[i+1] := 0C;
		END;
		RETURN;
	END;
	Done := FALSE;
  END MakeFileName;

  PROCEDURE ReadInt(VAR integ : INTEGER);
  CONST
    	SAFELIMITDIV10 = MAX(INTEGER) DIV 10;
    	SAFELIMITREM10 = MAX(INTEGER) MOD 10;
  TYPE
	itype = [0..31];
	ibuf =  ARRAY itype OF CHAR;
  VAR
    	int : INTEGER;
    	neg : BOOLEAN;
    	safedigit: [0 .. 9];
    	chvalue: CARDINAL;
	buf : ibuf;
	index : itype;
  BEGIN
    	ReadString(buf);
	IF NOT Done THEN
		RETURN
	END;
	index := 0;
    	IF buf[index] = '-' THEN
		neg := TRUE;
		INC(index);
    	ELSIF buf[index] = '+' THEN
		neg := FALSE;
		INC(index);
    	ELSE
		neg := FALSE
    	END;

    	safedigit := SAFELIMITREM10;
    	IF neg THEN safedigit := safedigit + 1 END;
    	int := 0;
	WHILE (buf[index] >= '0') & (buf[index] <= '9') DO
  		chvalue := ORD(buf[index]) - ORD('0');
	   	IF (int > SAFELIMITDIV10) OR 
		   ( (int = SAFELIMITDIV10) AND
		     (chvalue > safedigit)) THEN
			Message("integer too large");
			HALT;
	    	ELSE
			int := 10*int + VAL(INTEGER, chvalue);
			INC(index)
	    	END;
	END;
	IF neg THEN
   		integ := -int
	ELSE
		integ := int
	END;
	IF buf[index] > " " THEN
		Message("illegal integer");
		HALT;
	END;
	Done := TRUE;
  END ReadInt;

  PROCEDURE ReadCard(VAR card : CARDINAL);
  CONST
    	SAFELIMITDIV10 = MAX(CARDINAL) DIV 10;
    	SAFELIMITREM10 = MAX(CARDINAL) MOD 10;

  TYPE
	itype = [0..31];
	ibuf =  ARRAY itype OF CHAR;
    
  VAR
    	int : CARDINAL;
    	index  : itype;
	buf : ibuf;
    	safedigit: [0 .. 9];
    	chvalue: CARDINAL;
  BEGIN
    	ReadString(buf);
	IF NOT Done THEN RETURN; END;
	index := 0;
    	safedigit := SAFELIMITREM10;
    	int := 0;
	WHILE (buf[index] >= '0') & (buf[index] <= '9') DO
  		chvalue := ORD(buf[index]) - ORD('0');
	    	IF (int > SAFELIMITDIV10) OR 
		   ( (int = SAFELIMITDIV10) AND
		     (chvalue > safedigit)) THEN
			Message("cardinal too large");
			HALT;
	    	ELSE
			int := 10*int + chvalue;
			INC(index);
	    	END;
	END;
	IF buf[index] > " " THEN
		Message("illegal cardinal");
		HALT;
	END;
	card := int;
	Done := TRUE;
  END ReadCard;

  PROCEDURE ReadString(VAR s : ARRAY OF CHAR);
  TYPE charset = SET OF CHAR;
  VAR	i : CARDINAL;
    	ch : CHAR;

  BEGIN
    	i := 0;
	REPEAT
		Read(ch);
	UNTIL NOT (ch IN charset{' ', TAB, 12C, 15C});
	IF NOT Done THEN
		RETURN;
	END;
	UnRead(ch);
    	REPEAT
		Read(ch);
		termCH := ch;
		IF i <= HIGH(s) THEN
			s[i] := ch;
			IF (NOT Done) OR (ch <= " ") THEN
				s[i] := 0C;
			END;
		END;
		INC(i);
    	UNTIL (NOT Done) OR (ch <= " ");
	IF Done THEN UnRead(ch); END;
  END ReadString;

  PROCEDURE XReadString(VAR s : ARRAY OF CHAR);
  VAR	j : CARDINAL;
    	ch : CHAR;

  BEGIN
	j := 0;
	LOOP
		Streams.Read(Streams.InputStream, ch, result);
		IF result # Streams.succeeded THEN
			EXIT;
		END;
		IF ch <= " " THEN
			s[j] := 0C;
			EXIT;
		END;
		IF j < HIGH(s) THEN
			s[j] := ch;
			INC(j);
		END;
	END;
  END XReadString;

  PROCEDURE XWriteString(s: ARRAY OF CHAR);
  VAR i: CARDINAL;
  BEGIN
	i := 0;
	LOOP
		IF (i <= HIGH(s)) AND (s[i] # 0C) THEN
			Streams.Write(Streams.OutputStream, s[i], result);
			INC(i);
		ELSE
			EXIT;
		END;
	END;
  END XWriteString;

  PROCEDURE WriteCard(card, width : CARDINAL);
  VAR
    	buf : numbuf;
  BEGIN
	ConvertCardinal(card, width, buf);
	WriteString(buf);
  END WriteCard;

  PROCEDURE WriteInt(int : INTEGER; width : CARDINAL);
  VAR
    	buf : numbuf;
  BEGIN
    	ConvertInteger(int, width, buf);
	WriteString(buf);
  END WriteInt;

  PROCEDURE WriteHex(card, width : CARDINAL);
  VAR
    	buf : numbuf;
  BEGIN
	ConvertHex(card, width, buf);
	WriteString(buf);
  END WriteHex;

  PROCEDURE WriteLn;
  BEGIN
    	Write(EOL)
  END WriteLn;

  PROCEDURE WriteOct(card, width : CARDINAL);
  VAR
    	buf : numbuf;
  BEGIN
    	ConvertOctal(card, width, buf);
	WriteString(buf);
  END WriteOct;

  PROCEDURE WriteString(str : ARRAY OF CHAR);
  VAR
    	nbytes : CARDINAL;
  BEGIN
    	nbytes := 0;
    	WHILE (nbytes <= HIGH(str)) AND (str[nbytes] # 0C) DO
		Write(str[nbytes]);
		INC(nbytes)
    	END;
  END WriteString;

BEGIN	(* InOut initialization *)
	CurrIn := Streams.InputStream;
	CurrOut := Streams.OutputStream;
	unread := FALSE;
END InOut.

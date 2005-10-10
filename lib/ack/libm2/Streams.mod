#
(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE Streams;
(*
  Module:       Stream Input/Output
  Author:       Ceriel J.H. Jacobs
  Version:      $Header$

  Implementation for Unix
*)

  FROM	SYSTEM IMPORT	BYTE, ADR;
  FROM	Epilogue IMPORT	CallAtEnd;
  FROM	Storage IMPORT	Allocate, Available;
  FROM	StripUnix IMPORT
			open, close, lseek, read, write, creat;
  IMPORT StripUnix;

  CONST BUFSIZ = 1024;	(* tunable *)
  TYPE	IOB = RECORD
		kind: StreamKind;
		mode: StreamMode;
		eof: BOOLEAN;
		buffering: StreamBuffering;
		next : Stream;
		fildes: INTEGER;
		cnt, maxcnt: INTEGER;
		bufferedcnt: INTEGER;
		buf: ARRAY[1..BUFSIZ] OF BYTE;
	      END;
	Stream = POINTER TO IOB;
  VAR
	ibuf, obuf, ebuf: IOB;
	head: Stream;

  PROCEDURE getstruct(VAR stream: Stream);
  BEGIN
        stream := head;
        WHILE (stream # NIL) AND (stream^.kind # none) DO
                stream := stream^.next;
        END;
        IF stream = NIL THEN
		IF NOT Available(SIZE(IOB)) THEN
			RETURN;
		END;
                Allocate(stream,SIZE(IOB));
                stream^.next := head;
                head := stream;
        END;
  END getstruct;
 
  PROCEDURE freestruct(stream: Stream);
  BEGIN
	stream^.kind := none;
  END freestruct;

  PROCEDURE OpenStream(VAR stream: Stream;
		       filename: ARRAY OF CHAR; 
		       kind: StreamKind;
		       mode: StreamMode;
		       VAR result: StreamResult);
    VAR fd: INTEGER;
	i: CARDINAL;
  BEGIN
	IF kind = none THEN
		result := illegaloperation;
		RETURN;
	END;
	getstruct(stream);
	IF stream = NIL THEN
		result := nomemory;
		RETURN;
	END;
        WITH stream^ DO
                FOR i := 0 TO HIGH(filename) DO
                        buf[i+1] := BYTE(filename[i]);
                END;
                buf[HIGH(filename)+2] := BYTE(0C);
	END;
	IF (mode = reading) THEN
		fd := open(ADR(stream^.buf), 0);
	ELSE
		fd := -1;
		IF (mode = appending) THEN
			fd := open(ADR(stream^.buf), 1);
			IF fd >= 0 THEN
				IF (lseek(fd, 0D , 2) < 0D) THEN ; END;
			END;
		END;
		IF fd < 0 THEN
			fd := creat(ADR(stream^.buf), 666B);
		END;
	END;
	IF fd < 0 THEN
		result := openfailed;
		freestruct(stream);
		stream := NIL;
		RETURN;
	END;
	result := succeeded;
	stream^.fildes := fd;
	stream^.kind := kind;
	stream^.mode := mode;
	stream^.buffering := blockbuffered;
	stream^.bufferedcnt := BUFSIZ;
	stream^.maxcnt := 0;
	stream^.eof := FALSE;
	IF mode = reading THEN
		stream^.cnt := 1;
	ELSE
		stream^.cnt := 0;
	END;
  END OpenStream;

  PROCEDURE SetStreamBuffering( stream: Stream;
				b: StreamBuffering;
				VAR result: StreamResult);
  BEGIN
	result := succeeded;
	IF (stream = NIL) OR (stream^.kind = none) THEN
		result := nostream;
		RETURN;
	END;
	IF (stream^.mode = reading) OR
	   ((b = linebuffered) AND (stream^.kind = binary)) THEN
		result := illegaloperation;
		RETURN;
	END;
	FlushStream(stream, result);
	IF b = unbuffered THEN
		stream^.bufferedcnt := 1;
	END;
	stream^.buffering := b;
  END SetStreamBuffering;

  PROCEDURE FlushStream(stream: Stream; VAR result: StreamResult);
  VAR cnt1: INTEGER;
  BEGIN
	result := succeeded;
	IF (stream = NIL) OR (stream^.kind = none) THEN
		result := nostream;
		RETURN;
	END;
	WITH stream^ DO
		IF mode = reading THEN
			result := illegaloperation;
			RETURN;
		END;
		IF (cnt > 0) THEN
			cnt1 := cnt;
			cnt := 0;
			IF write(fildes, ADR(buf), cnt1) < 0 THEN END;
		END;
	END;
  END FlushStream;

  PROCEDURE CloseStream(VAR stream: Stream; VAR result: StreamResult);
  BEGIN
	IF (stream # NIL) AND (stream^.kind # none) THEN
		result := succeeded;
		IF stream^.mode # reading THEN
			FlushStream(stream, result);
		END;
		IF close(stream^.fildes) < 0 THEN ; END;
		freestruct(stream);
	ELSE
		result := nostream;
	END;
	stream := NIL;
  END CloseStream;
	
  PROCEDURE EndOfStream(stream: Stream; VAR result: StreamResult): BOOLEAN;
  BEGIN
	result := succeeded;
	IF (stream = NIL) OR (stream^.kind = none) THEN
		result := nostream;
		RETURN FALSE;
	END;
	IF stream^.mode # reading THEN
		result := illegaloperation;
		RETURN FALSE;
	END;
	IF stream^.eof THEN RETURN TRUE; END;
	RETURN (CHAR(NextByte(stream)) = 0C) AND stream^.eof;
  END EndOfStream;

  PROCEDURE FlushLineBuffers();
  VAR	s: Stream;
	result: StreamResult;
  BEGIN
	s := head;
	WHILE s # NIL DO
		IF (s^.kind # none) AND (s^.buffering = linebuffered) THEN
			FlushStream(s, result);
		END;
		s := s^.next;
	END;
  END FlushLineBuffers;

  PROCEDURE NextByte(stream: Stream): BYTE;
  VAR c: BYTE;
  BEGIN
	WITH stream^ DO
		IF cnt <= maxcnt THEN
			c := buf[cnt];
		ELSE
			IF eof THEN RETURN BYTE(0C); END;
			IF stream = InputStream THEN
				FlushLineBuffers();
			END;
			maxcnt := read(fildes, ADR(buf), bufferedcnt);
			cnt := 1;
			IF maxcnt <= 0 THEN
				eof := TRUE;
				c := BYTE(0C);
			ELSE
				c := buf[1];
			END;
		END;
	END;
	RETURN c;
  END NextByte;

  PROCEDURE Read(stream: Stream; VAR ch: CHAR; VAR result: StreamResult);
  VAR EoF: BOOLEAN;
  BEGIN
	ch := 0C;
	EoF := EndOfStream(stream, result);
	IF result # succeeded THEN RETURN; END;
	IF EoF THEN
		result := endoffile;
		RETURN;
	END;
	WITH stream^ DO
		ch := CHAR(buf[cnt]);
		INC(cnt);
	END;
  END Read;

  PROCEDURE ReadByte(stream: Stream; VAR byte: BYTE; VAR result: StreamResult);
  VAR EoF: BOOLEAN;
  BEGIN
	byte := BYTE(0C);
	EoF := EndOfStream(stream, result);
	IF result # succeeded THEN RETURN; END;
	IF EoF THEN
		result := endoffile;
		RETURN;
	END;
	WITH stream^ DO
		byte := buf[cnt];
		INC(cnt);
	END;
  END ReadByte;

  PROCEDURE ReadBytes(stream: Stream;
		      VAR bytes: ARRAY OF BYTE;
		      VAR result: StreamResult);
  VAR i: CARDINAL;
  BEGIN
	FOR i := 0 TO HIGH(bytes) DO
		ReadByte(stream, bytes[i], result);
	END;
  END ReadBytes;

  PROCEDURE Write(stream: Stream; ch: CHAR; VAR result: StreamResult);
  BEGIN
	IF (stream = NIL) OR (stream^.kind = none) THEN
		result := nostream;
		RETURN;
	END;
	IF (stream^.kind # text) OR (stream^.mode = reading) THEN
		result := illegaloperation;
		RETURN;
	END;
	WITH stream^ DO
		INC(cnt);
		buf[cnt] := BYTE(ch);
		IF (cnt >= bufferedcnt) OR
		   ((ch = 12C) AND (buffering = linebuffered))
		THEN
			FlushStream(stream, result);
		END;
	END;
  END Write;

  PROCEDURE WriteByte(stream: Stream; byte: BYTE; VAR result: StreamResult);
  BEGIN
	IF (stream = NIL) OR (stream^.kind = none) THEN
		result := nostream;
		RETURN;
	END;
	IF (stream^.kind # binary) OR (stream^.mode = reading) THEN
		result := illegaloperation;
		RETURN;
	END;
	WITH stream^ DO
		INC(cnt);
		buf[cnt] := byte;
		IF cnt >= bufferedcnt THEN
			FlushStream(stream, result);
		END;
	END;
  END WriteByte;

  PROCEDURE WriteBytes(stream: Stream; bytes: ARRAY OF BYTE; VAR result: StreamResult);
  VAR i: CARDINAL;
  BEGIN
	FOR i := 0 TO HIGH(bytes) DO
		WriteByte(stream, bytes[i], result);
	END;
  END WriteBytes;

  PROCEDURE EndIt;
  VAR h, h1 : Stream;
      result: StreamResult;
  BEGIN
	h := head;
	WHILE h # NIL DO
		h1 := h;
		CloseStream(h1, result);
		h := h^.next;
	END;
  END EndIt;

  PROCEDURE GetPosition(s: Stream; VAR position: LONGINT;
			VAR result: StreamResult);
  BEGIN
	IF (s = NIL) OR (s^.kind = none) THEN
		result := illegaloperation;
		RETURN;
	END;
	IF (s^.mode # reading) THEN FlushStream(s, result); END;
	position := lseek(s^.fildes, 0D, 1);
	IF position < 0D THEN
		result := illegaloperation;
		RETURN;
	END;
	IF s^.mode = reading THEN
		position := position + LONG(s^.maxcnt - s^.cnt + 1);
	END;
  END GetPosition;

  PROCEDURE SetPosition(s: Stream; position: LONGINT; VAR result: StreamResult);
  VAR currpos: LONGINT;
  BEGIN
	currpos := 0D;
	IF (s = NIL) OR (s^.kind = none) THEN
		result := nostream;
		RETURN;
	END;
	IF (s^.mode # reading) THEN
		FlushStream(s, result);
	ELSE
		s^.maxcnt := 0;
		s^.eof := FALSE;
	END;
	IF s^.mode = appending THEN
		currpos := lseek(s^.fildes, 0D, 1);
		IF currpos < 0D THEN
			result := illegaloperation;
			RETURN;
		END;
	END;
	IF position < currpos THEN
		result := illegaloperation;
		RETURN;
	END;
	currpos := lseek(s^.fildes, position, 0);
	IF currpos < 0D THEN
		result := illegaloperation;
		RETURN;
	END;
	result := succeeded;
  END SetPosition;

  PROCEDURE isatty(stream: Stream; VAR result: StreamResult): BOOLEAN;
  BEGIN
	IF (stream = NIL) OR (stream^.kind = none) THEN
		result := nostream;
		RETURN FALSE;
	END;
	RETURN StripUnix.isatty(stream^.fildes);
  END isatty;

  PROCEDURE InitStreams;
  VAR result: StreamResult;
  BEGIN
	InputStream := ADR(ibuf);
	OutputStream := ADR(obuf);
	ErrorStream := ADR(ebuf);
	WITH ibuf DO
		kind := text;
		mode := reading;
		eof := FALSE;
		next := ADR(obuf);
		fildes := 0;
		maxcnt := 0;
		cnt := 1;
		bufferedcnt := BUFSIZ;
	END;
	WITH obuf DO
		kind := text;
		mode := writing;
		eof := TRUE;
		next := ADR(ebuf);
		fildes := 1;
		maxcnt := 0;
		cnt := 0;
		bufferedcnt := BUFSIZ;
		IF isatty(OutputStream, result) THEN
			buffering := linebuffered;
		ELSE
			buffering := blockbuffered;
		END;
	END;
	WITH ebuf DO
		kind := text;
		mode := writing;
		eof := TRUE;
		next := NIL;
		fildes := 2;
		maxcnt := 0;
		cnt := 0;
		bufferedcnt := BUFSIZ;
		IF isatty(ErrorStream, result) THEN
			buffering := linebuffered;
		ELSE
			buffering := blockbuffered;
		END;
	END;
	head := InputStream;
	IF CallAtEnd(EndIt) THEN ; END;
  END InitStreams;

BEGIN
	InitStreams
END Streams.

(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE Strings;
(*
  Module:       String manipulations
  Author:       Ceriel J.H. Jacobs
  Version:      $Header$
*)

  PROCEDURE Assign(source: ARRAY OF CHAR; VAR dest: ARRAY OF CHAR);
  (* Assign string source to dest
  *)
  VAR	i: CARDINAL;
	max: CARDINAL;
  BEGIN
	max := HIGH(source);
	IF HIGH(dest) < max THEN max := HIGH(dest); END;
	i := 0;
	WHILE (i <= max) AND (source[i] # 0C) DO
		dest[i] := source[i];
		INC(i);
	END;
	IF i < HIGH(dest) THEN dest[i] := 0C; END;
  END Assign;

  PROCEDURE Insert(substr: ARRAY OF CHAR; VAR str: ARRAY OF CHAR; inx: CARDINAL);
  (* Insert the string substr into str, starting at str[inx].
     If inx is equal to or greater than Length(str) then substr is appended
     to the end of str.
  *)
  VAR	sublen, length, i: CARDINAL;
  BEGIN
	sublen := Length(substr);
	IF sublen = 0 THEN RETURN; END;
	length := Length(str);
	IF inx > length THEN inx := length; END;
	i := length;
	IF i + sublen  - 1 > HIGH(str) THEN i := HIGH(str); END;
	WHILE i > inx DO
		str[i+sublen-1] := str[i-1];
		DEC(i);
	END;
	FOR i := 0 TO sublen - 1 DO
		IF i + inx <= HIGH(str) THEN
			str[i + inx] := substr[i];
		ELSE
			RETURN;
		END;
	END;
	IF length + sublen <= HIGH(str) THEN
		str[length + sublen] := 0C;
	END;
  END Insert;

  PROCEDURE Delete(VAR str: ARRAY OF CHAR; inx, len: CARDINAL);
  (* Delete len characters from str, starting at str[inx].
     If inx >= Length(str) then nothing happens.
     If there are not len characters to delete, characters to the end of the
     string are deleted.
  *)
  VAR	length: CARDINAL;
  BEGIN
	IF len = 0 THEN RETURN; END;
	length := Length(str);
	IF inx >= length THEN RETURN; END;
	WHILE inx + len < length DO
		str[inx] := str[inx + len];
		INC(inx);
	END;
	str[inx] := 0C;
  END Delete;

  PROCEDURE Pos(substr, str: ARRAY OF CHAR): CARDINAL;
  (* Return the index into str of the first occurrence of substr.
     Pos returns a value greater than HIGH(str) of no occurrence is found.
  *)
  VAR	i, j, max, subl: CARDINAL;
  BEGIN
	max := Length(str);
	subl := Length(substr);
	IF subl > max THEN RETURN HIGH(str) + 1; END;
	IF subl = 0 THEN RETURN 0; END;
	max := max - subl;
	FOR i := 0 TO max DO
		j := 0;
		WHILE (j <= subl-1) AND (str[i+j] = substr[j]) DO
			INC(j);
		END;
		IF j = subl THEN RETURN i; END;
	END;
	RETURN HIGH(str) + 1;
  END Pos;

  PROCEDURE Copy(str: ARRAY OF CHAR;
	         inx, len: CARDINAL;
	         VAR result: ARRAY OF CHAR);
  (* Copy at most len characters from str into result, starting at str[inx].
  *)
  VAR	i: CARDINAL;
  BEGIN
	IF Length(str) <= inx THEN RETURN END;
	i := 0;
	LOOP
		IF i > HIGH(result) THEN RETURN; END;
		IF len = 0 THEN EXIT; END;
		IF inx > HIGH(str) THEN EXIT; END;
		result[i] := str[inx];
		INC(i); INC(inx); DEC(len);
	END;
	IF i <= HIGH(result) THEN result[i] := 0C; END;
  END Copy;

  PROCEDURE Concat(s1, s2: ARRAY OF CHAR; VAR result: ARRAY OF CHAR);
  (* Concatenate two strings.
  *)
  VAR	i, j: CARDINAL;
  BEGIN
	i := 0;
	WHILE (i <= HIGH(s1)) AND (s1[i] # 0C) DO
		IF i > HIGH(result) THEN RETURN END;
		result[i] := s1[i];
		INC(i);
	END;
	j := 0;
	WHILE (j <= HIGH(s2)) AND (s2[j] # 0C) DO
		IF i > HIGH(result) THEN RETURN END;
		result[i] := s2[j];
		INC(i);
		INC(j);
	END;
	IF i <= HIGH(result) THEN result[i] := 0C; END;
  END Concat;

  PROCEDURE Length(str: ARRAY OF CHAR): CARDINAL;
  (* Return number of characters in str.
  *)
  VAR i: CARDINAL;
  BEGIN
	i := 0;
	WHILE (i <= HIGH(str)) DO
		IF str[i] = 0C THEN RETURN i; END;
		INC(i);
	END;
	RETURN i;
  END Length;

  PROCEDURE CompareStr(s1, s2: ARRAY OF CHAR): INTEGER;
  (* Compare two strings, return -1 if s1 < s2, 0 if s1 = s2, and 1 if s1 > s2.
  *)
  VAR	i: CARDINAL;
	max: CARDINAL;
  BEGIN
	max := HIGH(s1);
	IF HIGH(s2) < max THEN max := HIGH(s2); END;
	i := 0;
	WHILE (i <= max) DO
		IF s1[i] < s2[i] THEN RETURN -1; END;
		IF s1[i] > s2[i] THEN RETURN 1; END;
		IF s1[i] = 0C THEN RETURN 0; END;
		INC(i);
	END;
	IF (i <= HIGH(s1)) AND (s1[i] # 0C) THEN RETURN 1; END;
	IF (i <= HIGH(s2)) AND (s2[i] # 0C) THEN RETURN -1; END;
	RETURN 0;
  END CompareStr;

END Strings.

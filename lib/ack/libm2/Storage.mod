(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE Storage;
(*
  Module:	Dynamic Storage Allocation
  Author:	Ceriel J.H. Jacobs
		Adapted from a version in C by Hans Tebra
  Version:	$Header$
*)
(* This storage manager maintains an array of lists of objects with the
   same size. Commonly used sizes have their own bucket. The larger ones
   are put in a single list.
*)
  FROM	Unix IMPORT	sbrk, ILLBREAK;
  FROM	SYSTEM IMPORT	ADDRESS, ADR;
  FROM	Traps IMPORT	Message;

  CONST
	NLISTS = 20;
	MAGICW = 0A5A5H;
	MAGICC = 175C;

  TYPE
	ALIGNTYPE = 
	  RECORD
		CASE : INTEGER OF
		  1: l: LONGINT |
		  2: p: ADDRESS |
		  3: d: LONGREAL
		END
	  END;			(* A type with high alignment requirements *)
	BucketPtr = POINTER TO Bucket;
	Bucket =
	  RECORD
		CASE : BOOLEAN OF
		   FALSE:
			  BNEXT: BucketPtr; 	(* next free Bucket *)
			  BSIZE: CARDINAL; |	(* size of user part in UNITs *)
		   TRUE: BXX: ALIGNTYPE
		END;
		BSTORE: ALIGNTYPE;
	  END;

  CONST
	UNIT = SIZE(ALIGNTYPE);

  VAR
	FreeLists: ARRAY[0..NLISTS] OF BucketPtr;	(* small blocks *)
	Llist: BucketPtr;				(* others *)
	Compacted: BOOLEAN;		(* avoid recursive reorganization *)
	FirstBlock: BucketPtr;
	USED: ADDRESS;

  PROCEDURE MyAllocate(size: CARDINAL) : ADDRESS;
    VAR	nu : CARDINAL;
	b : CARDINAL;
	p, q: BucketPtr;
	pc: POINTER TO CHAR;
	brk : ADDRESS;
  BEGIN
	IF size > CARDINAL(MAX(INTEGER)-2*UNIT + 1) THEN
		RETURN NIL;
	END;
	nu := (size + (UNIT-1)) DIV UNIT;
	IF nu = 0 THEN
		nu := 1;
	END;
	IF nu <= NLISTS THEN
		b := nu;
		IF FreeLists[b] # NIL THEN
			(* Exact fit *)
			p := FreeLists[b];
			FreeLists[b] := p^.BNEXT;
			p^.BNEXT := USED;
			IF p^.BSIZE * UNIT # size THEN
				pc := ADR(p^.BSTORE) + size;
				pc^ := MAGICC;
			END;
			p^.BSIZE := size;
			RETURN ADR(p^.BSTORE);
		END;

		(* Search for a block with >= 2 units more than requested.
		   We pay for an additional header when the block is split.
		*)
		FOR b := b+2 TO NLISTS DO
			IF FreeLists[b] # NIL THEN
				q := FreeLists[b];
				FreeLists[b] := q^.BNEXT;
				p := ADDRESS(q) + (nu+1)*UNIT;
				(* p indicates the block that must be given
				   back
				*)
				p^.BSIZE := q^.BSIZE - nu - 1;
				p^.BNEXT := FreeLists[p^.BSIZE];
				FreeLists[p^.BSIZE] := p;
				q^.BSIZE := nu;
				q^.BNEXT := USED;
				IF q^.BSIZE * UNIT # size THEN
					pc := ADR(q^.BSTORE) + size;
					pc^ := MAGICC;
				END;
				q^.BSIZE := size;
				RETURN ADR(q^.BSTORE);
			END;
		END;
	END;

	p := Llist;
	IF p # NIL THEN
		q := NIL;
		WHILE (p # NIL) AND (p^.BSIZE < nu) DO
			q := p;
			p := p^.BNEXT;
		END;

		IF p # NIL THEN
			(* p^.BSIZE >= nu *)
			IF p^.BSIZE <= nu + NLISTS + 1 THEN
				(* Remove p from this list *)
				IF q # NIL THEN q^.BNEXT := p^.BNEXT
				ELSE Llist := p^.BNEXT;
				END;
				p^.BNEXT := USED;
				IF p^.BSIZE > nu + 1 THEN
					(* split block,
					   tail goes to FreeLists area
					*)
					q := ADDRESS(p) + (nu+1)*UNIT;
					q^.BSIZE := p^.BSIZE -nu -1;
					q^.BNEXT := FreeLists[q^.BSIZE];
					FreeLists[q^.BSIZE] := q;
					p^.BSIZE := nu;
				END;
				IF p^.BSIZE * UNIT # size THEN
					pc := ADR(p^.BSTORE) + size;
					pc^ := MAGICC;
				END;
				p^.BSIZE := size;
				RETURN ADR(p^.BSTORE);
			END;
			(* Give part of tail of original block.
			   Block stays in this list.
			*)
			q := ADDRESS(p) + (p^.BSIZE-nu)*UNIT;
			q^.BSIZE := nu;
			p^.BSIZE := p^.BSIZE - nu - 1;
			q^.BNEXT := USED;
			IF q^.BSIZE * UNIT # size THEN
				pc := ADR(q^.BSTORE) + size;
				pc^ := MAGICC;
			END;
			q^.BSIZE := size;
			RETURN ADR(q^.BSTORE);
		END;
	END;

	IF Compacted THEN
		(* reorganization did not yield sufficient memory *)
		RETURN NIL;
	END;

	brk := sbrk(UNIT * (nu + 1));
	IF brk = ILLBREAK THEN
		ReOrganize();
		Compacted := TRUE;
		brk := MyAllocate(size);
		Compacted := FALSE;
		RETURN brk;
	END;

	p := brk;
	p^.BSIZE := nu;
	p^.BNEXT := USED;
	IF p^.BSIZE * UNIT # size THEN
		pc := ADR(p^.BSTORE) + size;
		pc^ := MAGICC;
	END;
	p^.BSIZE := size;
	RETURN ADR(p^.BSTORE);
  END MyAllocate;

  PROCEDURE ALLOCATE(VAR a: ADDRESS; size: CARDINAL);
  BEGIN
	Allocate(a, size);
  END ALLOCATE;

  PROCEDURE Allocate(VAR a: ADDRESS; size: CARDINAL);
  BEGIN
	a := MyAllocate(size);
	IF a = NIL THEN
		Message("out of core");
		HALT;
	END;
  END Allocate;

  PROCEDURE Available(size: CARDINAL): BOOLEAN;
    VAR	a: ADDRESS;
  BEGIN
	a:= MyAllocate(size);
	IF a # NIL THEN
		Deallocate(a, size);
		RETURN TRUE;
	END;
	RETURN FALSE;
  END Available;

  PROCEDURE DEALLOCATE(VAR a: ADDRESS; size: CARDINAL);
  BEGIN
	Deallocate(a, size);
  END DEALLOCATE;

  PROCEDURE Deallocate(VAR a: ADDRESS; size: CARDINAL);
    VAR	p: BucketPtr;
	pc: POINTER TO CHAR;
  BEGIN
	IF (a = NIL) THEN 
		Message("(Warning) Deallocate: NIL pointer deallocated");
		RETURN;
	END;
	p := a - UNIT;
	IF (p^.BNEXT # BucketPtr(USED)) THEN
		Message("(Warning) Deallocate: area already deallocated or heap corrupted");
		a := NIL;
		RETURN;
	END;
	WITH p^ DO
		IF BSIZE # size THEN
			Message("(Warning) Deallocate: wrong size or heap corrupted");
		END;
		BSIZE := (size + (UNIT - 1)) DIV UNIT;
		IF (BSIZE*UNIT # size) THEN
			pc := a + size;
			IF pc^ # MAGICC THEN
				Message("(Warning) Deallocate: heap corrupted");
			END;
		END;	
		IF BSIZE <= NLISTS THEN
			BNEXT := FreeLists[BSIZE];
			FreeLists[BSIZE] := p;
		ELSE
			BNEXT := Llist;
			Llist := p;
		END;
	END;
	a := NIL
  END Deallocate;

  PROCEDURE ReOrganize();
    VAR lastblock: BucketPtr;
	b, be: BucketPtr;
	i: CARDINAL;
  BEGIN
	lastblock := NIL;
	FOR i := 1 TO NLISTS DO
		b := FreeLists[i];
		WHILE b # NIL DO
			IF ADDRESS(b) > ADDRESS(lastblock) THEN
				lastblock := b;
			END;
			be := b^.BNEXT;
			b^.BNEXT := NIL;	(* temporary free mark *)
			b := be;
		END;
	END;

	b := Llist;
	WHILE b # NIL DO
		IF ADDRESS(b) > ADDRESS(lastblock) THEN
			lastblock := b;
		END;
		be := b^.BNEXT;
		b^.BNEXT := NIL;
		b := be;
	END;

	(* Now, all free blocks have b^.BNEXT = NIL *)

	b := FirstBlock;
	WHILE ADDRESS(b) < ADDRESS(lastblock) DO
		LOOP
			be := ADDRESS(b)+(b^.BSIZE+1)*UNIT;
			IF b^.BNEXT # NIL THEN	
				(* this block is not free *)
				EXIT;
			END;
			IF ADDRESS(be) > ADDRESS(lastblock) THEN
				(* no next block *)
				EXIT;
			END;
			IF be^.BNEXT # NIL THEN
				(* next block is not free *)
				EXIT;
			END;
			(* this block and the next one are free,
			   so merge them, but only if it is not too big
			*)
			IF MAX(CARDINAL) - b^.BSIZE > be^.BSIZE THEN
				b^.BSIZE := b^.BSIZE + be^.BSIZE + 1;
			ELSE
				EXIT;
			END;
		END;
		b := be;
	END;

	(* clear all free lists *)
	FOR i := 1 TO NLISTS DO FreeLists[i] := NIL; END;
	Llist := NIL;

	(* collect free blocks in them again *)
	b := FirstBlock;
	WHILE ADDRESS(b) <= ADDRESS(lastblock) DO
		WITH b^ DO
			IF BNEXT = NIL THEN
				IF BSIZE <= NLISTS THEN
					BNEXT := FreeLists[BSIZE];
					FreeLists[BSIZE] := b;
				ELSE
					BNEXT := Llist;
					Llist := b;
				END;
				b := ADDRESS(b) + (BSIZE+1) * UNIT;
			ELSE
				b := ADDRESS(b) + 
					((BSIZE + (UNIT - 1)) DIV UNIT + 1) * UNIT;
			END;
		END;
	END;
  END ReOrganize;

  PROCEDURE InitStorage();
    VAR	i: CARDINAL;
	brk: ADDRESS;
  BEGIN
	FOR i := 1 TO NLISTS DO
		FreeLists[i] := NIL;
	END;
	Llist := NIL;
	brk := sbrk(0);
	brk := sbrk(UNIT - brk MOD UNIT);
	FirstBlock := sbrk(0);
	Compacted := FALSE;
	USED := MAGICW;
  END InitStorage;

BEGIN
	InitStorage();
END Storage.

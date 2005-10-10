(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE ArraySort;
(* 
  Module:	Array sorting module.
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*)
  FROM	SYSTEM IMPORT	ADDRESS, BYTE;	(* no generics in Modula-2, sorry *)

  TYPE BytePtr = POINTER TO BYTE;

  VAR compareproc: CompareProc;

  PROCEDURE Sort(base: ADDRESS;		(* address of array *)
		 nel: CARDINAL;		(* number of elements in array *)
		 size: CARDINAL;	(* size of each element *)
		 compar: CompareProc);	(* the comparison procedure *)
  BEGIN
	compareproc := compar;
	qsort(base, base+(nel-1)*size, size);
  END Sort;

  PROCEDURE qsort(a1, a2: ADDRESS; size: CARDINAL);
  (* Implemented with quick-sort, with some extra's *)
    VAR	left, right, lefteq, righteq: ADDRESS;
	cmp: CompareResult;
	mainloop: BOOLEAN;
  BEGIN
	WHILE a2 > a1 DO
		left := a1;
		right := a2;
		lefteq := a1 + size * (((a2 - a1) + size) DIV (2 * size));
		righteq := lefteq;
		(*
                   Pick an element in the middle of the array.
                   We will collect the equals around it.
                   "lefteq" and "righteq" indicate the left and right
                   bounds of the equals respectively.
                   Smaller elements end up left of it, larger elements end
                   up right of it.
		*)
		LOOP
			LOOP
				IF left >= lefteq THEN EXIT END;
			      	cmp := compareproc(left, lefteq);
				IF cmp = greater THEN EXIT END;
				IF cmp = less THEN
					left := left + size;
				ELSE
					(* equal, so exchange with the element
					   to the left of the "equal"-interval.
					*)
					lefteq := lefteq - size;
					exchange(left, lefteq, size);
				END;
			END;
			mainloop := FALSE;
			LOOP
				IF right <= righteq THEN EXIT END;
				cmp := compareproc(right, righteq);
				IF cmp = less THEN
					IF left < lefteq THEN
						(* larger one at the left,
						   so exchange
						*)
						exchange(left,right,size);
						left := left + size;
						right := right - size;
						mainloop := TRUE;
						EXIT;
					END;
                                (*
 				   no more room at the left part, so we
                                   move the "equal-interval" one place to the
                                   right, and the smaller element to the
                                   left of it.
                                   This is best expressed as a three-way
                                   exchange.
                                *)
					righteq := righteq + size;
					threewayexchange(left, righteq, right,
						size);
					lefteq := lefteq + size;
					left := lefteq;
				ELSIF cmp = equal THEN
					(* equal, zo exchange with the element
					   to the right of the "equal"
					   interval
					*)
					righteq := righteq + size;
					exchange(right, righteq, size);
				ELSE
					(* leave it where it is *)
					right := right - size;
				END;
			END;
			IF (NOT mainloop) THEN
				IF left >= lefteq THEN
					(* sort "smaller" part *)
					qsort(a1, lefteq - size, size);
					(* and now the "larger" part, saving a
					   procedure call, because of this big
					   WHILE loop
					*)
					a1 := righteq + size;
					EXIT;	(* from the LOOP *)
				END;
                        	(* larger element to the left, but no more room,
                      	     	   so move the "equal-interval" one place to the
                           	   left, and the larger element to the right
                           	   of it.
                         	*)
				lefteq := lefteq - size;
				threewayexchange(right, lefteq, left, size);
				righteq := righteq - size;
				right := righteq;
			END;
		END;
	END;
  END qsort;

  PROCEDURE exchange(a,b: BytePtr; size : CARDINAL);
    VAR c: BYTE;
  BEGIN
	WHILE size > 0 DO
		DEC(size);
		c := a^;
		a^ := b^;
		a := ADDRESS(a) + 1;
		b^ := c;
		b := ADDRESS(b) + 1;
	END;
  END exchange;

  PROCEDURE threewayexchange(p,q,r: BytePtr; size: CARDINAL);
    VAR c: BYTE;
  BEGIN
	WHILE size > 0 DO
		DEC(size);
		c := p^;
		p^ := r^;
		p := ADDRESS(p) + 1;
		r^ := q^;
		r := ADDRESS(r) + 1;
		q^ := c;
		q := ADDRESS(q) + 1;
	END;
  END threewayexchange;

END ArraySort.

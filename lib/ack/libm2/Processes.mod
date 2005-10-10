(*$R-*)
IMPLEMENTATION MODULE Processes [1];
(*
  Module:       Processes
  From:         "Programming in Modula-2", 3rd, corrected edition, by N. Wirth
  Version:      $Header$
*)

  FROM	SYSTEM IMPORT	ADDRESS, TSIZE, NEWPROCESS, TRANSFER;
  FROM	Storage IMPORT	Allocate;
  FROM	Traps IMPORT	Message;

  TYPE	SIGNAL = POINTER TO ProcessDescriptor;

	ProcessDescriptor =
		RECORD	next: SIGNAL;	(* ring *)
			queue: SIGNAL;	(* queue of waiting processes *)
			cor: ADDRESS;
			ready: BOOLEAN;
		END;

  VAR	cp: SIGNAL;			(* current process *)

  PROCEDURE StartProcess(P: PROC; n: CARDINAL);
    VAR	s0: SIGNAL;
	wsp: ADDRESS;
  BEGIN
	s0 := cp;
	Allocate(wsp, n);
	Allocate(cp, TSIZE(ProcessDescriptor));
	WITH cp^ DO
		next := s0^.next;
		s0^.next := cp;
		ready := TRUE;
		queue := NIL
	END;
	NEWPROCESS(P, wsp, n, cp^.cor);
	TRANSFER(s0^.cor, cp^.cor);
  END StartProcess;

  PROCEDURE SEND(VAR s: SIGNAL);
    VAR	s0: SIGNAL;
  BEGIN
	IF s # NIL THEN
		s0 := cp;
		cp := s;
		WITH cp^ DO
			s := queue;
			ready := TRUE;
			queue := NIL
		END;
		TRANSFER(s0^.cor, cp^.cor);
	END
  END SEND;

  PROCEDURE WAIT(VAR s: SIGNAL);
    VAR	s0, s1: SIGNAL;
  BEGIN
	(* insert cp in queue s *)
	IF s = NIL THEN
		s := cp
	ELSE
		s0 := s;
		s1 := s0^.queue;
		WHILE s1 # NIL DO
			s0 := s1;
			s1 := s0^.queue
		END;
		s0^.queue := cp
	END;
	s0 := cp;
	REPEAT
		cp := cp^.next
	UNTIL cp^.ready;
	IF cp = s0 THEN
		(* deadlock *)
		Message("deadlock");
		HALT
	END;
	s0^.ready := FALSE;
	TRANSFER(s0^.cor, cp^.cor)
  END WAIT;

  PROCEDURE Awaited(s: SIGNAL): BOOLEAN;
  BEGIN
	RETURN s # NIL
  END Awaited;

  PROCEDURE Init(VAR s: SIGNAL);
  BEGIN
	s := NIL
  END Init;

BEGIN
	Allocate(cp, TSIZE(ProcessDescriptor));
	WITH cp^ DO
		next := cp;
		ready := TRUE;
		queue := NIL
	END
END Processes.

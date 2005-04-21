(*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*)

(*$R-*)
IMPLEMENTATION MODULE Semaphores [1];
(*
  Module:       Processes with semaphores
  Author:       Ceriel J.H. Jacobs
  Version:      $Header$

  Quasi-concurrency implementation
*)

  FROM	SYSTEM IMPORT	ADDRESS, NEWPROCESS, TRANSFER;
  FROM	Storage IMPORT	Allocate;
  FROM	random IMPORT	Uniform;
  FROM	Traps IMPORT	Message;

  TYPE	Sema = POINTER TO Semaphore;
	Processes = POINTER TO Process;
	Semaphore =
		RECORD
			level: CARDINAL;
		END;
	Process =
		RECORD	next: Processes;
			proc: ADDRESS;
			waiting: Sema;
		END;

  VAR	cp: Processes;			(* current process *)

  PROCEDURE StartProcess(P: PROC; n: CARDINAL);
    VAR	s0: Processes;
	wsp: ADDRESS;
  BEGIN
	s0 := cp;
	Allocate(wsp, n);
	Allocate(cp, SIZE(Process));
	WITH cp^ DO
		next := s0^.next;
		s0^.next := cp;
		waiting := NIL;
	END;
	NEWPROCESS(P, wsp, n, cp^.proc);
	TRANSFER(s0^.proc, cp^.proc);
  END StartProcess;

  PROCEDURE Up(VAR s: Sema);
  BEGIN
	s^.level := s^.level + 1;
	ReSchedule;
  END Up;

  PROCEDURE Down(VAR s: Sema);
  BEGIN
	IF s^.level = 0 THEN
		cp^.waiting := s;
	ELSE
		s^.level := s^.level - 1;
	END;
	ReSchedule;
  END Down;

  PROCEDURE NewSema(n: CARDINAL): Sema;
  VAR	s: Sema;
  BEGIN
	Allocate(s, SIZE(Semaphore));
	s^.level := n;
	RETURN s;
  END NewSema;

  PROCEDURE Level(s: Sema): CARDINAL;
  BEGIN
	RETURN s^.level;
  END Level;

  PROCEDURE ReSchedule;
  VAR s0: Processes;
      i, j: CARDINAL;
  BEGIN
	s0 := cp;
	i := Uniform(1, 5);
	j := i;
	LOOP
		cp := cp^.next;
		IF Runnable(cp) THEN
			DEC(i);
			IF i = 0 THEN EXIT END;
		END;
		IF (cp = s0) AND (j = i) THEN
			(* deadlock *)
			Message("deadlock");
			HALT
		END;
	END;
	IF cp # s0 THEN TRANSFER(s0^.proc, cp^.proc); END;
  END ReSchedule;

  PROCEDURE Runnable(p: Processes): BOOLEAN;
  BEGIN
	IF p^.waiting = NIL THEN RETURN TRUE; END;
	IF p^.waiting^.level > 0 THEN
		p^.waiting^.level := p^.waiting^.level - 1;
		p^.waiting := NIL;
		RETURN TRUE;
	END;
	RETURN FALSE;
  END Runnable;
BEGIN
	Allocate(cp, SIZE(Process));
	WITH cp^ DO
		next := cp;
		waiting := NIL;
	END
END Semaphores.

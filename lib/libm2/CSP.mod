(*$R-*)
IMPLEMENTATION MODULE CSP;
(*
  Module:	Communicating Sequential Processes
  From:		"A Modula-2 Implementation of CSP",
		M. Collado, R. Morales, J.J. Moreno,
		SIGPlan Notices, Volume 22, Number 6, June 1987.
		Some modifications by Ceriel J.H. Jacobs
  Version:	$Header$

   See this article for an explanation of the use of this module.
*)

  FROM random	IMPORT	Uniform;
  FROM SYSTEM	IMPORT	BYTE, ADDRESS, NEWPROCESS, TRANSFER;
  FROM Storage	IMPORT	Allocate, Deallocate;
  FROM Traps	IMPORT	Message;

  CONST	WorkSpaceSize = 2000;

  TYPE	ByteAddress =	POINTER TO BYTE;
	Channel =	POINTER TO ChannelDescriptor;
	ProcessType =	POINTER TO ProcessDescriptor;
	ProcessDescriptor = RECORD
				next: ProcessType;
				father: ProcessType;
				cor: ADDRESS;
				wsp: ADDRESS;
				guardindex: INTEGER;
				guardno: CARDINAL;
				guardcount: CARDINAL;
				opened: Channel;
				sons: CARDINAL;
				msgadr: ADDRESS;
				msglen: CARDINAL;
			    END;

	Queue =	RECORD
		    head, tail: ProcessType;
		END;

	ChannelDescriptor = RECORD
				senders: Queue;
				owner: ProcessType;
				guardindex: INTEGER;
				next: Channel;
			    END;

  VAR	cp: ProcessType;
	free, ready: Queue;

(* ------------ Private modules and procedures ------------- *)

  MODULE ProcessQueue;

    IMPORT	ProcessType, Queue;
    EXPORT	Push, Pop, InitQueue, IsEmpty;

    PROCEDURE InitQueue(VAR q: Queue);
    BEGIN
	WITH q DO
		head := NIL;
		tail := NIL
	END
    END InitQueue;

    PROCEDURE Push(p: ProcessType; VAR q: Queue);
    BEGIN
	p^.next := NIL;
	WITH q DO
		IF head = NIL THEN
			tail := p
		ELSE
			head^.next := p
		END;
		head := p
	END
    END Push;

    PROCEDURE Pop(VAR q: Queue; VAR p: ProcessType);
    BEGIN
	WITH q DO
		p := tail;
		IF p # NIL THEN
			tail := tail^.next;
			IF head = p THEN
				head := NIL
			END
		END
	END
    END Pop;

    PROCEDURE IsEmpty(q: Queue): BOOLEAN;
    BEGIN
	RETURN q.head = NIL
    END IsEmpty;

  END ProcessQueue;


  PROCEDURE DoTransfer;
    VAR	aux: ProcessType;
  BEGIN
	aux := cp;
	Pop(ready, cp);
	IF cp = NIL THEN
		HALT
	ELSE
		TRANSFER(aux^.cor, cp^.cor)
	END
  END DoTransfer;

  PROCEDURE OpenChannel(ch: Channel; n: INTEGER);
  BEGIN
	WITH ch^ DO
		IF guardindex = 0 THEN
			guardindex := n;
			next := cp^.opened;
			cp^.opened := ch
		END
	END
  END OpenChannel;

  PROCEDURE CloseChannels(p: ProcessType);
  BEGIN
	WITH p^ DO
		WHILE opened # NIL DO
			opened^.guardindex := 0;
			opened := opened^.next
		END
	END
  END CloseChannels;

  PROCEDURE ThereAreOpenChannels(): BOOLEAN;
  BEGIN
	RETURN cp^.opened # NIL;
  END ThereAreOpenChannels;

  PROCEDURE Sending(ch: Channel): BOOLEAN;
  BEGIN
	RETURN NOT IsEmpty(ch^.senders)
  END Sending;

(* -------------- Public Procedures ----------------- *)

  PROCEDURE COBEGIN;
  (* Beginning of a COBEGIN .. COEND structure *)
  BEGIN
  END COBEGIN;

  PROCEDURE COEND;
  (* End of a COBEGIN .. COEND structure *)
    (* VAR	aux: ProcessType; *)
  BEGIN
	IF cp^.sons > 0 THEN
		DoTransfer
	END
  END COEND;

  PROCEDURE StartProcess(P: PROC);
  (* Start an anonimous process that executes the procedure P *)
    VAR newprocess: ProcessType;
  BEGIN
	Pop(free, newprocess);
	IF newprocess = NIL THEN
		Allocate(newprocess,SIZE(ProcessDescriptor));
		Allocate(newprocess^.wsp, WorkSpaceSize)
	END;
	WITH newprocess^ DO
		father := cp;
		sons := 0;
		msglen := 0;
		NEWPROCESS(P, wsp, WorkSpaceSize, cor)
	END;
	cp^.sons := cp^.sons + 1;
	Push(newprocess, ready)
  END StartProcess;

  PROCEDURE StopProcess;
  (* Terminate a Process (itself) *)
    VAR aux: ProcessType;
  BEGIN
	aux := cp^.father;
	aux^.sons := aux^.sons - 1;
	IF aux^.sons = 0 THEN
		Push(aux, ready)
	END;
	aux := cp;
	Push(aux, free);
	Pop(ready, cp);
	IF cp = NIL THEN
		HALT
	ELSE
		TRANSFER(aux^.cor, cp^.cor)
	END
  END StopProcess;

  PROCEDURE InitChannel(VAR ch: Channel);
  (* Initialize the channel ch *)
  BEGIN
	Allocate(ch, SIZE(ChannelDescriptor));
	WITH ch^ DO
		InitQueue(senders);
		owner := NIL;
		next := NIL;
		guardindex := 0
	END
  END InitChannel;

  PROCEDURE GetChannel(ch: Channel);
  (* Assign the channel ch to the process that gets it *)
  BEGIN
	WITH ch^ DO
		IF owner # NIL THEN
			Message("Channel already has an owner");
			HALT
		END;
		owner := cp
	END
  END GetChannel;

  PROCEDURE Send(data: ARRAY OF BYTE; VAR ch: Channel);
  (* Send a message with the data to the cvhannel ch *)
    VAR	m: ByteAddress;
	(* aux: ProcessType; *)
	i: CARDINAL;
  BEGIN
	WITH ch^ DO
		Push(cp, senders);
		Allocate(cp^.msgadr, SIZE(data));
		m := cp^.msgadr;
		cp^.msglen := HIGH(data);
		FOR i := 0 TO HIGH(data) DO
			m^ := data[i];
			m := ADDRESS(m) + 1
		END;
		IF guardindex # 0 THEN
			owner^.guardindex := guardindex;
			CloseChannels(owner);
			Push(owner, ready)
		END
	END;
	DoTransfer
  END Send;

  PROCEDURE Receive(VAR ch: Channel; VAR dest: ARRAY OF BYTE);
  (* Receive a message from the channel ch into the dest variable *)
    VAR	aux: ProcessType;
	m: ByteAddress;
	i: CARDINAL;
  BEGIN
	WITH ch^ DO
		IF cp # owner THEN
			Message("Only owner of channel can receive from it");
			HALT
		END;
		IF Sending(ch) THEN
			Pop(senders, aux);
			m := aux^.msgadr;
			FOR i := 0 TO aux^.msglen DO
				dest[i] := m^;
				m := ADDRESS(m) + 1
			END;
			Push(aux, ready);
			Push(cp, ready);
			CloseChannels(cp)
		ELSE
			OpenChannel(ch, -1);
			DoTransfer;
			Pop(senders, aux);
			m := aux^.msgadr;
			FOR i := 0 TO aux^.msglen DO
				dest[i] := m^;
				m := ADDRESS(m) + 1
			END;
			Push(cp, ready);
			Push(aux, ready)
		END;
		Deallocate(aux^.msgadr, aux^.msglen+1);
		DoTransfer
	END
  END Receive;

  PROCEDURE SELECT(n: CARDINAL);
  (* Beginning of a SELECT structure with n guards *)
  BEGIN
	cp^.guardindex := Uniform(1,n);
	cp^.guardno := n;
	cp^.guardcount := n
  END SELECT;

  PROCEDURE NEXTGUARD(): CARDINAL;
  (* Returns an index to the next guard to be evaluated in a SELECT *)
  BEGIN
	RETURN cp^.guardindex
  END NEXTGUARD;

  PROCEDURE GUARD(cond: BOOLEAN; ch: Channel;
		  VAR dest: ARRAY OF BYTE): BOOLEAN;
  (* Evaluates a guard, including reception management *)
    (* VAR	aux: ProcessType; *)
  BEGIN
	IF NOT cond THEN
		RETURN FALSE
	ELSIF ch = NIL THEN
		CloseChannels(cp);
		cp^.guardindex := 0;
		RETURN TRUE
	ELSIF Sending(ch) THEN
		Receive(ch, dest);
		cp^.guardindex := 0;
		RETURN TRUE
	ELSE
		OpenChannel(ch, cp^.guardindex);
		RETURN FALSE
	END
  END GUARD;

  PROCEDURE ENDSELECT(): BOOLEAN;
  (* End of a SELECT structure *)
  BEGIN
	WITH cp^ DO
		IF guardindex <= 0 THEN
			RETURN TRUE
		END;
		guardcount := guardcount - 1;
		IF guardcount # 0 THEN
			guardindex := (guardindex MOD INTEGER(guardno)) + 1
		ELSIF ThereAreOpenChannels() THEN
			DoTransfer
		ELSE
			guardindex := 0
		END
	END;
	RETURN FALSE
  END ENDSELECT;

BEGIN
	InitQueue(free);
	InitQueue(ready);
	Allocate(cp,SIZE(ProcessDescriptor));
	WITH cp^ DO
		sons := 0;
		father := NIL
	END
END CSP.


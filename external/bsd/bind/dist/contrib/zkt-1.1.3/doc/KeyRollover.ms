.NH 1
DNS Key Status Types and Filenames
.PP
.TS
cfB	| cfB	  s	| cfB	s	| cfB	| cfB
cfB	| cfB	| cfB	| cfB	| cfB	| cfB	| cfB
l	| l	| n	| l	| l	| c	| lfCW	.
Status	Key	Filename	used for	dnssec-zkt
\^	Type	Flags	public	private	signing?	label
_
active	ZSK	256	.key	.private	y	act ive
	KSK	257	.key	.private	y	act ive
.sp 0.2
published	ZSK	256	.key	.published	n	pub lished
	KSK	257	.key	.private	n	sta ndby
.sp 0.2
depreciated (retired)	ZSK	256	.key	.depreciated	n	dep reciated
.sp 0.2
revoked	KSK	385	.key	.private	y	rev oked
.sp 0.2
removed	KSK	257	k*.key	k*.private	n	-
.sp 0.2
sep	KSK	257	.key	-	n	sep
.ig
.sp 0.2
(master	KSK	257	M...key	.private	n	-)
..
.TE
.SP 2
.NH 1
Key rollover 
.PP
.NH 2
Zone signing key rollover (pre-publish RFC4641)
.PP
.TS
rfB	 cfB	|cfB	|cfB	|cfB
lfB	|cfB	|cfB	|cfB	|cfB
l	|l	|l	|l	|l	.
action		create	change	remove
keys		newkey	sig key	old key
_
zsk1	active	active	depreciated	
zsk2		published	active	active
.sp 0.3
RRSIG	zsk1	zsk1	zsk2	zsk2	
.TE
.SP 2
.NH 2
Key signing key rollover (double signature RFC4641)
.PP
.TS
rfB	 cfB	|cfB	|cfB	|cfB
lfB	|cfB	|cfB	|cfB	|cfB
l	|l	|l	|l	|l	.
action		create	change	remove
keys		newkey	delegation	old key
_
ksk\d1\u	active	active	active	
ksk\d2\u		active	active	active
.sp 0.3
DNSKEY RRSIG	ksk1	ksk1,ksk2	ksk1,ksk2	ksk2	
.sp 0.3
DS at parent	DS\d1\u	DS\d1\u	DS\d2\u	DS\d2\u
.TE
.\"RRSIG	DNSKEY\dksk1\u	DNSKEY\dksk1,ksk2\u	DNSKEY\dksk1,ksk2\u	DNSKEY\dksk2\u	
.SP 2
.NH 2
Key signing key rollover (rfc5011)
.PP
.TS
rfB	 cfB	|cfB	|cfB
lfB	|cfB	|cfB	|cfB
l	|l	|l	|l	.
action		newkey 	change delegation
keys		& rollover	& remove old key
_
ksk\d1\u	active	revoke\v'-0.2'\(dg\v'+0.2'		
ksk\d2\u	standby	active	active
ksk\d3\u		standby\v'-0.2'\(dd\v'+0.2'	standby
.sp 0.3
DNSKEY RRSIG	ksk1	ksk1,ksk2	ksk2
.sp 0.3
Parent DS	DS\d1\u	DS\d1\u	DS\d2\u
	DS\d2\u	DS\d2\u	DS\d3\u
.TE
.LP
\v'-0.2'\(dg\v'0.2'
Have to remain until the remove hold-down time is expired,
which is 30days at a minimum.
.LP
\v'-0.2'\(dd\v'0.2'
Will be the standby key after the hold-down time is expired
.br
Add holdtime \(eq max(30days, TTL of DNSKEY)

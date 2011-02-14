$set 1
$ EPERM
1 Bewerking niet toegestaan
$ ENOENT
2 Bestand of directory niet gevonden
$ ESRCH
3 Taak bestaat niet
$ EINTR
4 Onderbroken systeemaanroep
$ EIO
5 Invoer/uitvoer fout
$ ENXIO
6 Apparaat niet geconfigureerd
$ E2BIG
7 Argumenten lijst is te lang
$ ENOEXEC
8 Programma kan niet worden uitgevoerd
$ EBADF
9 Ongeldige bestandsverwijzing
$ ECHILD
10 Geen kind processen
$ EDEADLK
11 Een "deadlock" is vermeden
$ ENOMEM
12 Kan geen geheugen meer verkrijgen
$ EACCES
13 Toegang geweigerd
$ EFAULT
14 Ongeldig adres
$ ENOTBLK
15 Een per blok adresseerbaar apparaat is vereist
$ EBUSY
16 Apparaat is bezig
$ EEXIST
17 Bestand bestaat reeds
$ EXDEV
18 Verwijzing tussen bestanden op verschillende bestandssystemen
$ ENODEV
19 Bewerking wordt niet ondersteund door dit apparaat
$ ENOTDIR
20 Dit is geen directory
$ EISDIR
21 Dit is een directory
$ EINVAL
22 Ongeldig argument
$ ENFILE
23 Te veel open bestanden in het systeem
$ EMFILE
24 Te veel open bestanden
$ ENOTTY
25 ioctl niet van toepassing op dit apparaat
$ ETXTBSY
26 Programma bestand is bezig
$ EFBIG
27 Bestand is te groot
$ ENOSPC
28 Geen ruimte meer op dit apparaat
$ ESPIPE
29 Onuitvoerbare zoekopdracht
$ EROFS
30 Van dit bestandssysteem kan alleen worden gelezen
$ EMLINK
31 Te veel bestandsverwijzingen
$ EPIPE
32 Verbroken pijp
$ EDOM
33 Numeriek argument valt buiten domein
$ ERANGE
34 Resultaat te groot of te klein
$ EAGAIN, EWOULDBLOCK
35 Middel tijdelijk onbeschikbaar
$ EINPROGRESS
36 Bewerking in gang gezet
$ EALREADY
37 Bewerking is al in gang gezet
$ ENOTSOCK
38 Voor deze bewerking is een contactpunt vereist
$ EDESTADDRREQ
39 Een bestemmingsadres is vereist
$ EMSGSIZE
40 Te grote boodschap
$ EPROTOTYPE
41 Protocol past niet bij dit contactpunt
$ ENOPROTOOPT
42 Protocol is niet beschikbaar
$ EPROTONOSUPPORT
43 Protocol is niet ondersteund
$ ESOCKTNOSUPPORT
44 Dit soort contactpunt is niet ondersteund
$ EOPNOTSUPP
45 Bewerking niet ondersteund
$ EPFNOSUPPORT
46 Protocol familie niet ondersteund
$ EAFNOSUPPORT
47 Adressen familie niet ondersteund door protocol familie
$ EADDRINUSE
48 Adres is al in gebruik
$ EADDRNOTAVAIL
49 Het gevraagde adres kan niet worden toegekend
$ ENETDOWN
50 Netwerk is plat
$ ENETUNREACH
51 Netwerk is onbereikbaar
$ ENETRESET
52 Netwerk onderbrak verbinding tijdens herstart
$ ECONNABORTED
53 Verbroken verbinding veroorzaakt door software
$ ECONNRESET
54 Verbinding werd aan de andere kant verbroken
$ ENOBUFS
55 Geen buffer ruimte meer beschikbaar
$ EISCONN
56 Dit contactpunt is al verbonden
$ ENOTCONN
57 Contactpunt is niet verbonden
$ ESHUTDOWN
58 Een afgesloten contactpunt kan geen gegevens meer verzenden
$ ETOOMANYREFS
59 Te veel verwijzingen: splitsen niet mogelijk
$ ETIMEDOUT
60 Verbinding te lang niet mogelijk
$ ECONNREFUSED
61 Verbinding geweigerd
$ ELOOP
62 Te veel niveaus van symbolische verwijzingen
$ ENAMETOOLONG
63 Bestandsnaam te lang
$ EHOSTDOWN
64 Bestemming niet actief
$ EHOSTUNREACH
65 Bestemming niet bereikbaar
$ ENOTEMPTY
66 Directory is niet leeg
$ EPROCLIM
67 Te veel taken
$ EUSERS
68 Te veel gebruikers
$ EDQUOT
69 Schijf quota overschreden
$ ESTALE
70 Verlopen NFS bestandsverwijzing
$ EREMOTE
71 Te veel verwijzingen op afstand in dit pad
$ EBADRPC
72 RPC argument structuur is incorrect
$ ERPCMISMATCH
73 RPC versie is verkeerd
$ EPROGUNAVAIL
74 RPC programma niet beschikbaar
$ EPROGMISMATCH
75 Programma versie is verkeerd
$ EPROCUNAVAIL
76 Taak kan door dit programma niet worden uitgevoerd
$ ENOLCK
77 Geen locks beschikbaar
$ ENOSYS
78 Deze systeem functie is niet geimplementeerd
$ EFTYPE
79 Bestandsformaat niet van toepassing
$ EAUTH
80 Aanmeldingsfout
$ ENEEDAUTH
81 Aanmeldingsprocedure benodigd
$ EIDRM
82 De aanwijzer is verwijderd
$ ENOMSG
83 Geen boodschap van het gewenste type
$ EOVERFLOW
84 Waarde te groot om te bewaren in data type
$ EILSEQ
85 Ongeldige byte reeks
$ ENOTSUP
86 Niet ondersteund
$ ECANCELED
87 Bewerking geannuleerd
$ EBADMSG
88 Verkeerde of defecte boodschap
$ ENODATA
89 Geen boodschap beschikbaar
$ ENOSR
90 Geen STREAM voorraad
$ ENOSTR
91 Dit is geen STREAM
$ ETIME
92 STREAM ioctl verlopen
$ ENOATTR
93 Attribuut niet gevonden
$ EMULTIHOP
94 Multihopverzoek
$ ENOLINK
95 Verbinding werd verstoord
$ EPROTO
96 Protocolfout
$set 2
$ SIGHUP
1 Opgehangen
$ SIGINT
2 Onderbroken
$ SIGQUIT
3 Opgegeven
$ SIGILL
4 Verboden instructie
$ SIGTRAP
5 Spoor/BPT val
$ SIGABRT
6 Abort val
$ SIGEMT
7 EMT val
$ SIGFPE
8 Drijvende komma fout
$ SIGKILL
9 Gedood
$ SIGBUS
10 Bus fout
$ SIGSEGV
11 Segmentatie fout
$ SIGSYS
12 Verkeerde systeemaanroep
$ SIGPIPE
13 Gebroken pijp
$ SIGALRM
14 Wekker
$ SIGTERM
15 Beeindigd
$ SIGURG
16 Dringende I/O opgemerkt
$ SIGSTOP
17 Gestopt (signaal)
$ SIGTSTP
18 Gestopt
$ SIGCONT
19 Voortgezet
$ SIGCHLD
20 Kindproces beeindigd
$ SIGTTIN
21 Gestopt (tty invoer)
$ SIGTTOU
22 Gestopt (tty uitvoer)
$ SIGIO
23 I/O mogelijk
$ SIGXCPU
24 Te veel CPU tijd verbruikt
$ SIGXFSZ
25 Maximum bestandsgrootte overschreden
$ SIGVTALRM
26 Virtuele wekker
$ SIGPROF
27 Profiling wekker
$ SIGWINCH
28 Venstergrootte veranderd
$ SIGINFO
29 Informatie verzoek
$ SIGUSR1
30 Gebruikersignaal 1
$ SIGUSR2
31 Gebruikersignaal 2
$ SIGPWR
32 Stroom uitval/inschakeling

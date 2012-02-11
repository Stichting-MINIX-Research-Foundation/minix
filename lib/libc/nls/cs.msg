$set 1
$ EPERM
1 Operace není povolena
$ ENOENT
2 Neexistující soubor nebo adresáø
$ ESRCH
3 Proces neexistuje
$ EINTR
4 Systémové volání pøeru¹eno
$ EIO
5 Vstupní/výstupní chyba
$ ENXIO
6 Zaøízení není nakonfigurováno
$ E2BIG
7 Seznam argumentù je pøíli¹ dlouhý
$ ENOEXEC
8 Chyba formátu spu¹tìného souboru
$ EBADF
9 ©patný souborový deskriptor
$ ECHILD
10 Neexistuje ¾ádný potomek procesu
$ EDEADLK
11 Zabránìno uváznutí (deadlock) prostøedku
$ ENOMEM
12 Nedostatek pamìti
$ EACCES
13 Pøístup odmítnut
$ EFAULT
14 Chybná adresa
$ ENOTBLK
15 Vy¾adováno blokové zaøízení
$ EBUSY
16 Zdroj nebo zaøízení je zaneprázdnìno
$ EEXIST
17 Soubor existuje
$ EXDEV
18 Odkaz vede na jiné zaøízení
$ ENODEV
19 Operace není zaøízením podporována
$ ENOTDIR
20 Toto není adresáø
$ EISDIR
21 Toto je adresáø
$ EINVAL
22 Chybný argument
$ ENFILE
23 Pøíli¹ mnoho otevøených souborù v systému
$ EMFILE
24 Pøíli¹ mnoho otevøených souborù
$ ENOTTY
25 Pro dané zaøízení nevhodné ioctl
$ ETXTBSY
26 S textovým souborem se pracuje
$ EFBIG
27 Soubor je pøíli¹ velký
$ ENOSPC
28 Na zaøízení není volné místo
$ ESPIPE
29 Neplatný lseek(2)
$ EROFS
30 Souborový systém je pouze pro ètení
$ EMLINK
31 Pøíli¹ mnoho odkazù
$ EPIPE
32 Pøeru¹ená roura
$ EDOM
33 Èíselný argument mimo definièní obor
$ ERANGE
34 Výsledek pøíli¹ velký nebo pøíli¹ malý
$ EAGAIN, EWOULDBLOCK
35 Prostøedek je doèasnì nedostupný
$ EINPROGRESS
36 Operace nyní probíhá
$ EALREADY
37 Operace ji¾ probíhá
$ ENOTSOCK
38 Socketová operace na neèem, co není socket
$ EDESTADDRREQ
39 Vy¾adována adresa cíle
$ EMSGSIZE
40 Zpráva je pøíli¹ dlouhá
$ EPROTOTYPE
41 Protokol nepodporuje daný typ socketu
$ ENOPROTOOPT
42 Protokol není dostupný
$ EPROTONOSUPPORT
43 Protokol není podporován
$ ESOCKTNOSUPPORT
44 Typ socketu není podporován
$ EOPNOTSUPP
45 Operace není podporována
$ EPFNOSUPPORT
46 Rodina protokolù není podporována
$ EAFNOSUPPORT
47 Rodina adres není protokolem podporována
$ EADDRINUSE
48 Adresa je ji¾ pou¾ívána
$ EADDRNOTAVAIL
49 Není mo¾no pøidìlit po¾adovanou adresu
$ ENETDOWN
50 Sí» je odpojena
$ ENETUNREACH
51 Sí» je nedosa¾itelná
$ ENETRESET
52 Sí» zru¹ila spojení po resetu
$ ECONNABORTED
53 Program zpùsobil ukonèení spojení
$ ECONNRESET
54 Spojení zru¹eno partnerem
$ ENOBUFS
55 Vyrovnávací pamì» není k dispozici
$ EISCONN
56 Socket je ji¾ u¾ pøipojen
$ ENOTCONN
57 Socket není pøipojen
$ ESHUTDOWN
58 Není mo¾no posílat po shutdownu socketu
$ ETOOMANYREFS
59 Pøíli¹ mnoho referencí: nelze spojit
$ ETIMEDOUT
60 Pøekroèen èasový limit pokusu o spojení
$ ECONNREFUSED
61 Spojení odmítnuto
$ ELOOP
62 Pøíli¹ mnoho úrovní symbolických odkazù
$ ENAMETOOLONG
63 Jméno souboru pøíli¹ dlouhé
$ EHOSTDOWN
64 Vzdálený uzel je odpojen
$ EHOSTUNREACH
65 Neexistuje cesta ke vzdálenému uzlu
$ ENOTEMPTY
66 Adresáø není prázdný
$ EPROCLIM
67 Pøíli¹ mnoho procesù
$ EUSERS
68 Pøíli¹ mnoho u¾ivatelù
$ EDQUOT
69 Disková kvóta pøekroèena
$ ESTALE
70 Propadnutý souborový ukazatel pro NFS
$ EREMOTE
71 Pøíli¹ mnoho úrovní vzdáleného v cestì
$ EBADRPC
72 ©patná RPC struktura
$ ERPCMISMATCH
73 RPC: Nekompatibilní verze RPC
$ EPROGUNAVAIL
74 RPC: Program není dostupný
$ EPROGMISMATCH
75 RPC: Neshoda programu nebo verze
$ EPROCUNAVAIL
76 RPC: Procedura není dostupná
$ ENOLCK
77 Zámky souborových záznamù nejsou dostupné
$ ENOSYS
78 Funkce není implementována
$ EFTYPE
79 Nevhodný typ nebo formát souboru
$ EAUTH
80 Chyba autentifikace
$ ENEEDAUTH
81 Je tøeba se autentifikovat
$ EIDRM
82 Identifikátor zru¹en
$ ENOMSG
83 Neexistuje zpráva ¾ádaného typu
$ EOVERFLOW
84 Hodnota je pro daný datový typ pøíli¹ velká
$ EILSEQ
85 Neplatná posloupnost bajtù
$ ENOTSUP
86 Nepodporováno
$ ECANCELED
87 Operace strornována
$ EBADMSG
88 ©patná nebo poru¹ená zpráva
$ ENODATA
89 ®ádná zpráva není dostupná
$ ENOSR
90 ®ádné STREAM prostøedky
$ ENOSTR
91 Není STREAM
$ ETIME
92 Vypr¹el èasový limit STREAM ioctl
$ ENOATTR
93 Atribut nebyl nalezen
$ EMULTIHOP
94 Pokus o spojení pøes více uzlù
$ ENOLINK
95 Odkaz byl zpøetrhán
$ EPROTO
96 Chyba protokolu
$set 2
$ SIGHUP
1 Terminál odpojen
$ SIGINT
2 Pøeru¹ení
$ SIGQUIT
3 Konec
$ SIGILL
4 Chybná instrukce
$ SIGTRAP
5 Ladící instrukce
$ SIGABRT
6 Násilné ukonèení
$ SIGEMT
7 Emulovaná instrukce
$ SIGFPE
8 Výjimka pohyblivé èárky
$ SIGKILL
9 Zabito
$ SIGBUS
10 Chyba sbìrnice
$ SIGSEGV
11 Chyba segmentace
$ SIGSYS
12 ©patné systémove volání
$ SIGPIPE
13 Broken pipe
$ SIGALRM
14 Alarm èasovaè
$ SIGTERM
15 Ukonèeno
$ SIGURG
16 Naléhavý I/O po¾adavek
$ SIGSTOP
17 Pozastaveno (signál)
$ SIGTSTP
18 Pozastaveno
$ SIGCONT
19 Pokraèování
$ SIGCHLD
20 Potomek procesu ukonèen
$ SIGTTIN
21 Pozastaveno (terminálový vstup)
$ SIGTTOU
22 Pozastaveno (terminálový výstup)
$ SIGIO
23 Vstup/výstup mo¾ný
$ SIGXCPU
24 Pøekroèen èasový limit pro CPU
$ SIGXFSZ
25 Pøekroèen limit velikosti souboru
$ SIGVTALRM
26 Vypr¹el virtuální èasovaè
$ SIGPROF
27 Vypr¹el profilovací èasovaè
$ SIGWINCH
28 Velikost okna zmìnìna
$ SIGINFO
29 Informaèní po¾adavek
$ SIGUSR1
30 U¾ivatelský signál 1
$ SIGUSR2
31 U¾ivatelský signál 2
$ SIGPWR
32 Selhání/nastartování napájení
